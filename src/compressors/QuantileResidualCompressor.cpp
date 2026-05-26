/// @file QuantileResidualCompressor.cpp
/// @brief Implementation of @ref QuantileResidualCompressor — see the header for option docs.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <stdexcept>

#include <zstd.h>

#include "QuantileResidualCompressor.hpp"

// ── Bit packing utilities ──────────────────────────────────────────
// Packs a stream of values where each value is (indexBits + residualBits) wide.
// We concatenate index and residual into a single code per point.

static void packBits(const std::vector<uint32_t>& codes, int nBits,
                     std::vector<uint8_t>& out) {
    size_t totalBits = codes.size() * static_cast<size_t>(nBits);
    size_t totalBytes = (totalBits + 7) / 8;
    out.resize(totalBytes, 0);

    uint64_t buffer = 0;
    int bitsInBuffer = 0;
    size_t bytePos = 0;

    for (uint32_t code : codes) {
        buffer = (buffer << nBits) | (code & (((uint64_t)1 << nBits) - 1));
        bitsInBuffer += nBits;

        while (bitsInBuffer >= 8) {
            bitsInBuffer -= 8;
            out[bytePos++] = static_cast<uint8_t>((buffer >> bitsInBuffer) & 0xFF);
        }
    }

    if (bitsInBuffer > 0) {
        out[bytePos] = static_cast<uint8_t>((buffer << (8 - bitsInBuffer)) & 0xFF);
    }
}

static std::vector<uint32_t> unpackBits(const uint8_t* data, size_t numCodes,
                                         int nBits) {
    std::vector<uint32_t> codes(numCodes);
    uint64_t mask = ((uint64_t)1 << nBits) - 1;

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

static int bitsNeeded(int nBins) {
    if (nBins <= 1) return 1;
    return static_cast<int>(std::ceil(std::log2(static_cast<double>(nBins))));
}

// ── Compressor implementation ──────────────────────────────────────

// Compressed buffer layout:
//   [0..1]    uint16   nBins
//   [2]       uint8    indexBits
//   [3]       uint8    residualBits
//   [4..7]    uint32   compressed edge block size (bytes)
//   [8..11]   uint32   raw edge block size (bytes)
//   [12..]    zstd-compressed bin edges (nBins+1 float32, delta-coded)
//   [...]     bit-packed codes (each code = indexBits + residualBits wide)

static constexpr size_t kHeaderSize = 12;

CompressedData QuantileResidualCompressor::compress(const std::vector<float>& data) {
    if (data.empty()) {
        return {.data = {}, .numFloats = 0};
    }

    const size_t n = data.size();
    const int nBins = std::min(_nBins, static_cast<int>(n));
    const int indexBits = bitsNeeded(nBins);
    const int residualBits = _residualBits;
    const int totalBitsPerPoint = indexBits + residualBits;
    const uint32_t nResidualLevels = 1u << residualBits;

    // ── Step 1: Compute quantile bin edges ──────────────────────
    std::vector<float> sorted(data);
    std::sort(sorted.begin(), sorted.end());

    std::vector<float> edges(nBins + 1);
    for (int i = 0; i < nBins; ++i) {
        size_t idx = static_cast<size_t>(
            static_cast<double>(i) * static_cast<double>(n) / static_cast<double>(nBins)
        );
        edges[i] = sorted[std::min(idx, n - 1)];
    }
    edges[nBins] = sorted[n - 1];

    // Ensure strict monotonicity
    for (int i = 1; i <= nBins; ++i) {
        if (edges[i] <= edges[i - 1]) {
            edges[i] = std::nextafter(edges[i - 1], std::numeric_limits<float>::max());
        }
    }

    // ── Step 2: Encode each point as (bin index, residual code) ─
    std::vector<uint32_t> codes(n);
    for (size_t i = 0; i < n; ++i) {
        // Find bin index via binary search
        auto it = std::upper_bound(edges.begin(), edges.end(), data[i]);
        int binIdx = static_cast<int>(it - edges.begin()) - 1;
        binIdx = std::clamp(binIdx, 0, nBins - 1);

        // Compute position within the bin [0, 1)
        float binLow = edges[binIdx];
        float binHigh = edges[binIdx + 1];
        float binWidth = binHigh - binLow;

        float position = 0.0f;
        if (binWidth > 0.0f) {
            position = (data[i] - binLow) / binWidth;
            position = std::clamp(position, 0.0f, 1.0f - 1e-7f);
        }

        // Quantize the position to residualBits
        auto residualCode = static_cast<uint32_t>(position * static_cast<float>(nResidualLevels));
        residualCode = std::min(residualCode, nResidualLevels - 1);

        // Pack bin index and residual into a single code
        codes[i] = (static_cast<uint32_t>(binIdx) << residualBits) | residualCode;
    }

    // ── Step 3: Delta-code and compress the bin edges ────────────
    std::vector<float> edgeDeltas(nBins + 1);
    edgeDeltas[0] = edges[0];
    for (int i = 1; i <= nBins; ++i) {
        edgeDeltas[i] = edges[i] - edges[i - 1];
    }

    size_t edgeRawSize = edgeDeltas.size() * sizeof(float);
    size_t edgeBound = ZSTD_compressBound(edgeRawSize);
    std::vector<uint8_t> edgeCompressed(edgeBound);
    size_t edgeCompSize = ZSTD_compress(
        edgeCompressed.data(), edgeBound,
        edgeDeltas.data(), edgeRawSize,
        _edgeCompressionLevel
    );
    if (ZSTD_isError(edgeCompSize)) {
        throw std::runtime_error(std::string("ZSTD edge compression failed: ") +
                                 ZSTD_getErrorName(edgeCompSize));
    }

    // ── Step 4: Bit-pack the combined codes ─────────────────────
    std::vector<uint8_t> packed;
    packBits(codes, totalBitsPerPoint, packed);

    // ── Step 5: Assemble output ─────────────────────────────────
    size_t totalSize = kHeaderSize + edgeCompSize + packed.size();
    std::vector<uint8_t> output(totalSize);

    auto nBins16 = static_cast<uint16_t>(nBins);
    auto edgeCompSize32 = static_cast<uint32_t>(edgeCompSize);
    auto edgeRawSize32 = static_cast<uint32_t>(edgeRawSize);

    std::memcpy(output.data() + 0, &nBins16, sizeof(uint16_t));
    output[2] = static_cast<uint8_t>(indexBits);
    output[3] = static_cast<uint8_t>(residualBits);
    std::memcpy(output.data() + 4, &edgeCompSize32, sizeof(uint32_t));
    std::memcpy(output.data() + 8, &edgeRawSize32, sizeof(uint32_t));
    std::memcpy(output.data() + kHeaderSize, edgeCompressed.data(), edgeCompSize);
    std::memcpy(output.data() + kHeaderSize + edgeCompSize,
                packed.data(), packed.size());

    return {
        .data = std::move(output),
        .numFloats = n
    };
}

std::vector<float> QuantileResidualCompressor::decompress(const CompressedData& compressedData) {
    if (compressedData.numFloats == 0) {
        return {};
    }

    // ── Read header ─────────────────────────────────────────────
    uint16_t nBins;
    uint8_t indexBits, residualBits;
    uint32_t edgeCompSize, edgeRawSize;

    std::memcpy(&nBins, compressedData.data.data() + 0, sizeof(uint16_t));
    indexBits = compressedData.data[2];
    residualBits = compressedData.data[3];
    std::memcpy(&edgeCompSize, compressedData.data.data() + 4, sizeof(uint32_t));
    std::memcpy(&edgeRawSize, compressedData.data.data() + 8, sizeof(uint32_t));

    int totalBitsPerPoint = indexBits + residualBits;
    uint32_t nResidualLevels = 1u << residualBits;
    uint32_t residualMask = nResidualLevels - 1;

    // ── Decompress and un-delta the bin edges ───────────────────
    std::vector<uint8_t> edgeRawBuf(edgeRawSize);
    size_t decompSize = ZSTD_decompress(
        edgeRawBuf.data(), edgeRawSize,
        compressedData.data.data() + kHeaderSize, edgeCompSize
    );
    if (ZSTD_isError(decompSize)) {
        throw std::runtime_error(std::string("ZSTD edge decompression failed: ") +
                                 ZSTD_getErrorName(decompSize));
    }

    size_t numEdges = edgeRawSize / sizeof(float);
    std::vector<float> edgeDeltas(numEdges);
    std::memcpy(edgeDeltas.data(), edgeRawBuf.data(), edgeRawSize);

    std::vector<float> edges(numEdges);
    edges[0] = edgeDeltas[0];
    for (size_t i = 1; i < numEdges; ++i) {
        edges[i] = edges[i - 1] + edgeDeltas[i];
    }

    // ── Unpack codes ────────────────────────────────────────────
    const uint8_t* packedData = compressedData.data.data() + kHeaderSize + edgeCompSize;
    auto codes = unpackBits(packedData, compressedData.numFloats, totalBitsPerPoint);

    // ── Reconstruct values ──────────────────────────────────────
    std::vector<float> output(compressedData.numFloats);
    for (size_t i = 0; i < compressedData.numFloats; ++i) {
        uint32_t binIdx = codes[i] >> residualBits;
        uint32_t residualCode = codes[i] & residualMask;

        float binLow = edges[binIdx];
        float binHigh = edges[binIdx + 1];

        // Midpoint reconstruction within the sub-bin
        float subPosition = (static_cast<float>(residualCode) + 0.5f)
                            / static_cast<float>(nResidualLevels);
        output[i] = binLow + subPosition * (binHigh - binLow);
    }

    return output;
}

void QuantileResidualCompressor::configure(const std::map<std::string, std::string>& options) {
    for (const auto& [key, value] : options) {
        if (key == "nBins") {
            _nBins = std::stoi(value);
            if (_nBins < 2 || _nBins > 65536) {
                throw std::invalid_argument("Invalid nBins. Must be between 2 and 65536.");
            }
        } else if (key == "residualBits") {
            _residualBits = std::stoi(value);
            if (_residualBits < 0 || _residualBits > 16) {
                throw std::invalid_argument("Invalid residualBits. Must be between 0 and 16.");
            }
        } else if (key == "edgeCompressionLevel") {
            _edgeCompressionLevel = std::stoi(value);
            if (_edgeCompressionLevel < 1 || _edgeCompressionLevel > 22) {
                throw std::invalid_argument("Invalid edgeCompressionLevel. Must be between 1 and 22.");
            }
        } else {
            throw std::invalid_argument("Unknown quantile-residual option: " + key);
        }
    }
}

std::map<std::string, std::string> QuantileResidualCompressor::getConfig() const {
    return {
        {"nBins", std::to_string(_nBins)},
        {"residualBits", std::to_string(_residualBits)},
        {"edgeCompressionLevel", std::to_string(_edgeCompressionLevel)}
    };
}

std::string QuantileResidualCompressor::name() const {
    return "quantile-residual";
}

std::string QuantileResidualCompressor::description() const {
    return "Quantile histogram with sub-bin residual coding. Bins data at equal-count "
           "quantiles (adapting to data distribution), then encodes each point as a "
           "bin index plus a B-bit residual giving the position within the bin. "
           "Effectively a two-level quantizer: coarse distribution-adaptive bins + "
           "fine uniform subdivision within each bin.";
}

std::string QuantileResidualCompressor::version() const {
    return "1.0.0";
}

std::string QuantileResidualCompressor::usage() const {
    return "Options:\n"
           "  nBins=<int>                Number of quantile bins (2-65536). Default is 256.\n"
           "  residualBits=<int>         Bits for sub-bin position (0-16). Default is 4.\n"
           "                             0 = same as quantile-histogram (midpoint only).\n"
           "  edgeCompressionLevel=<int> Zstd level for bin edges (1-22). Default is 3.\n"
           "\n"
           "Bits per point = ceil(log2(nBins)) + residualBits.\n"
           "Effective number of distinct levels = nBins * 2^residualBits.\n"
           "\n"
           "Examples:\n"
           "  nBins=256,  residualBits=4  → 12 bits/pt, 4096 effective levels, CR ~2.67x\n"
           "  nBins=128,  residualBits=4  → 11 bits/pt, 2048 effective levels, CR ~2.91x\n"
           "  nBins=64,   residualBits=4  → 10 bits/pt, 1024 effective levels, CR ~3.20x\n"
           "  nBins=32,   residualBits=4  →  9 bits/pt,  512 effective levels, CR ~3.56x\n";
}