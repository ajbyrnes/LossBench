/// @file UniformHistogramCompressor.cpp
/// @brief Implementation of @ref UniformHistogramCompressor — see the header for option docs.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include <zstd.h>

#include "UniformHistogramCompressor.hpp"

// ── Bit packing utilities ──────────────────────────────────────────
// Word-at-a-time packing: accumulate bits in a 64-bit register,
// flush full bytes as they fill. Much faster than bit-by-bit.

static void packBits(const std::vector<uint32_t>& codes, int nBits,
                     std::vector<uint8_t>& out) {
    size_t totalBits = codes.size() * static_cast<size_t>(nBits);
    size_t totalBytes = (totalBits + 7) / 8;
    out.resize(totalBytes, 0);

    uint64_t buffer = 0;
    int bitsInBuffer = 0;
    size_t bytePos = 0;

    for (uint32_t code : codes) {
        buffer = (buffer << nBits) | (code & ((1u << nBits) - 1));
        bitsInBuffer += nBits;

        // Flush complete bytes
        while (bitsInBuffer >= 8) {
            bitsInBuffer -= 8;
            out[bytePos++] = static_cast<uint8_t>((buffer >> bitsInBuffer) & 0xFF);
        }
    }

    // Flush remaining bits (left-aligned in the last byte)
    if (bitsInBuffer > 0) {
        out[bytePos] = static_cast<uint8_t>((buffer << (8 - bitsInBuffer)) & 0xFF);
    }
}

static std::vector<uint32_t> unpackBits(const uint8_t* data, size_t numCodes,
                                         int nBits) {
    std::vector<uint32_t> codes(numCodes);
    uint32_t mask = (1u << nBits) - 1;

    uint64_t buffer = 0;
    int bitsInBuffer = 0;
    size_t bytePos = 0;

    for (size_t i = 0; i < numCodes; ++i) {
        while (bitsInBuffer < nBits) {
            buffer = (buffer << 8) | data[bytePos++];
            bitsInBuffer += 8;
        }
        bitsInBuffer -= nBits;
        codes[i] = static_cast<uint32_t>((buffer >> bitsInBuffer) & mask);
    }

    return codes;
}

// ── Compressor implementation ──────────────────────────────────────

// Compressed buffer layout:
//   [0..3]    float    dataMin
//   [4..7]    float    dataMax
//   [8..9]    uint16   nBins
//   [10]      uint8    indexBits
//   [11..14]  uint32   uncompressed packed size (bytes)
//   [15..]    zstd-compressed bit-packed bin indices

static constexpr size_t kHeaderSize = 15;

static int bitsNeeded(int nBins) {
    if (nBins <= 1) return 1;
    return static_cast<int>(std::ceil(std::log2(static_cast<double>(nBins))));
}

CompressedData UniformHistogramCompressor::compress(const std::vector<float>& data) {
    if (data.empty()) {
        return {.data = {}, .numFloats = 0};
    }

    auto [minIt, maxIt] = std::minmax_element(data.begin(), data.end());
    float dataMin = *minIt;
    float dataMax = *maxIt;

    if (dataMax == dataMin) {
        dataMax = dataMin + 1.0f;
    }

    float range = dataMax - dataMin;
    int indexBits = bitsNeeded(_nBins);
    std::vector<uint32_t> indices(data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        float normalized = (data[i] - dataMin) / range;
        auto idx = static_cast<int>(normalized * _nBins);
        indices[i] = static_cast<uint32_t>(std::clamp(idx, 0, _nBins - 1));
    }

    // Bit-pack the indices
    std::vector<uint8_t> packed;
    packBits(indices, indexBits, packed);

    // Compress the packed bytes with zstd
    size_t zstdBound = ZSTD_compressBound(packed.size());
    std::vector<uint8_t> zstdBuf(zstdBound);
    size_t zstdSize = ZSTD_compress(zstdBuf.data(), zstdBound,
                                     packed.data(), packed.size(),
                                     _zstdLevel);
    if (ZSTD_isError(zstdSize)) {
        throw std::runtime_error(std::string("ZSTD compression failed: ") +
                                 ZSTD_getErrorName(zstdSize));
    }

    // Build output: header + zstd-compressed payload
    std::vector<uint8_t> output(kHeaderSize + zstdSize);
    std::memcpy(output.data() + 0, &dataMin, sizeof(float));
    std::memcpy(output.data() + 4, &dataMax, sizeof(float));
    auto nBins16 = static_cast<uint16_t>(_nBins);
    std::memcpy(output.data() + 8, &nBins16, sizeof(uint16_t));
    output[10] = static_cast<uint8_t>(indexBits);
    auto packedSize = static_cast<uint32_t>(packed.size());
    std::memcpy(output.data() + 11, &packedSize, sizeof(uint32_t));
    std::memcpy(output.data() + kHeaderSize, zstdBuf.data(), zstdSize);

    return {
        .data = std::move(output),
        .numFloats = data.size()
    };
}

std::vector<float> UniformHistogramCompressor::decompress(const CompressedData& compressedData) {
    if (compressedData.numFloats == 0) {
        return {};
    }

    // Read header
    float dataMin, dataMax;
    uint16_t nBins;
    uint8_t indexBits;
    uint32_t uncompressedSize;

    std::memcpy(&dataMin, compressedData.data.data() + 0, sizeof(float));
    std::memcpy(&dataMax, compressedData.data.data() + 4, sizeof(float));
    std::memcpy(&nBins, compressedData.data.data() + 8, sizeof(uint16_t));
    indexBits = compressedData.data[10];
    std::memcpy(&uncompressedSize, compressedData.data.data() + 11, sizeof(uint32_t));

    float binWidth = (dataMax - dataMin) / static_cast<float>(nBins);

    // Decompress the zstd payload
    const uint8_t* zstdData = compressedData.data.data() + kHeaderSize;
    size_t zstdSize = compressedData.data.size() - kHeaderSize;

    std::vector<uint8_t> packed(uncompressedSize);
    size_t decompSize = ZSTD_decompress(packed.data(), uncompressedSize,
                                         zstdData, zstdSize);
    if (ZSTD_isError(decompSize)) {
        throw std::runtime_error(std::string("ZSTD decompression failed: ") +
                                 ZSTD_getErrorName(decompSize));
    }

    // Unpack indices
    auto indices = unpackBits(packed.data(), compressedData.numFloats, indexBits);

    // Reconstruct as bin midpoints
    std::vector<float> output(compressedData.numFloats);
    for (size_t i = 0; i < compressedData.numFloats; ++i) {
        output[i] = dataMin + (static_cast<float>(indices[i]) + 0.5f) * binWidth;
    }

    return output;
}

void UniformHistogramCompressor::configure(const std::map<std::string, std::string>& options) {
    for (const auto& [key, value] : options) {
        if (key == "nBins") {
            _nBins = std::stoi(value);
            if (_nBins < 2 || _nBins > 65536) {
                throw std::invalid_argument("Invalid nBins. Must be between 2 and 65536.");
            }
        } else if (key == "zstdLevel") {
            _zstdLevel = std::stoi(value);
            if (_zstdLevel < 1 || _zstdLevel > 22) {
                throw std::invalid_argument("Invalid zstdLevel. Must be between 1 and 22.");
            }
        } else {
            throw std::invalid_argument("Unknown uniform-histogram option: " + key);
        }
    }
}

std::map<std::string, std::string> UniformHistogramCompressor::getConfig() const {
    return {
        {"nBins", std::to_string(_nBins)},
        {"zstdLevel", std::to_string(_zstdLevel)}
    };
}

std::string UniformHistogramCompressor::name() const {
    return "uniform-histogram";
}

std::string UniformHistogramCompressor::description() const {
    return "Uniform histogram lossy compressor with zstd entropy coding. "
           "Quantizes values into equally-spaced bins, then compresses the "
           "bin index stream with zstd.";
}

std::string UniformHistogramCompressor::version() const {
    return "2.0.0";
}

std::string UniformHistogramCompressor::usage() const {
    return "Options:\n"
           "  nBins=<int>      Number of histogram bins (2-65536). Default is 256.\n"
           "  zstdLevel=<int>  Zstd compression level (1-22). Default is 3.";
}