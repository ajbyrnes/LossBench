/// @file QuantileHistogramCompressor.cpp
/// @brief Implementation of @ref QuantileHistogramCompressor — see the header for option docs.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <stdexcept>

#include <zstd.h>

#include "QuantileHistogramCompressor.hpp"

// ── Bit packing utilities ──────────────────────────────────────────

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

static int bitsNeeded(int nBins) {
    if (nBins <= 1) return 1;
    return static_cast<int>(std::ceil(std::log2(static_cast<double>(nBins))));
}

// ── Compressor implementation ──────────────────────────────────────

// Compressed buffer layout:
//   [0..1]    uint16   nBins
//   [2]       uint8    indexBits
//   [3..6]    uint32   compressed edge block size (bytes)
//   [7..10]   uint32   raw edge block size (bytes, for decompression buffer)
//   [11..]    zstd-compressed bin edges (nBins+1 float32, delta-coded)
//   [...]     bit-packed bin indices (raw, not entropy coded — already max entropy)

static constexpr size_t kHeaderSize = 11;

CompressedData QuantileHistogramCompressor::compress(const std::vector<float>& data) {
    if (data.empty()) {
        return {.data = {}, .numFloats = 0};
    }

    const size_t n = data.size();
    const int nBins = std::min(_nBins, static_cast<int>(n));
    const int indexBits = bitsNeeded(nBins);

    // ── Step 1: Compute quantile bin edges ──────────────────────
    // Sort a copy of the data to find quantile positions
    std::vector<float> sorted(data);
    std::sort(sorted.begin(), sorted.end());

    // Build nBins+1 edges at equally-spaced quantile positions
    // Edge[i] = sorted value at position i * (n / nBins)
    // This ensures each bin has approximately the same number of points
    std::vector<float> edges(nBins + 1);
    for (int i = 0; i < nBins; ++i) {
        size_t idx = static_cast<size_t>(
            static_cast<double>(i) * static_cast<double>(n) / static_cast<double>(nBins)
        );
        edges[i] = sorted[std::min(idx, n - 1)];
    }
    edges[nBins] = sorted[n - 1];

    // Ensure strict monotonicity: nudge duplicate edges up by the
    // smallest representable increment. This handles plateaus in the data.
    for (int i = 1; i <= nBins; ++i) {
        if (edges[i] <= edges[i - 1]) {
            edges[i] = std::nextafter(edges[i - 1], std::numeric_limits<float>::max());
        }
    }

    // ── Step 2: Assign each point to a bin ──────────────────────
    // Binary search into edges for each original (unsorted) value
    std::vector<uint32_t> indices(n);
    for (size_t i = 0; i < n; ++i) {
        // upper_bound gives first edge > value; subtract 1 for bin index
        auto it = std::upper_bound(edges.begin(), edges.end(), data[i]);
        int idx = static_cast<int>(it - edges.begin()) - 1;
        indices[i] = static_cast<uint32_t>(std::clamp(idx, 0, nBins - 1));
    }

    // ── Step 3: Delta-code and compress the bin edges ────────────
    // Delta coding: store first edge as-is, then differences.
    // The differences are smooth and small → zstd compresses well.
    std::vector<float> edgeDeltas(nBins + 1);
    edgeDeltas[0] = edges[0];
    for (int i = 1; i <= nBins; ++i) {
        edgeDeltas[i] = edges[i] - edges[i - 1];
    }

    // Compress the delta-coded edges with zstd
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

    // ── Step 4: Bit-pack the indices (no entropy coding needed) ─
    // With quantile bins, all bins have ~equal occupancy, so
    // entropy ≈ log2(nBins). Raw packing is already near-optimal.
    std::vector<uint8_t> packedIndices;
    packBits(indices, indexBits, packedIndices);

    // ── Step 5: Assemble output ─────────────────────────────────
    size_t totalSize = kHeaderSize + edgeCompSize + packedIndices.size();
    std::vector<uint8_t> output(totalSize);

    auto nBins16 = static_cast<uint16_t>(nBins);
    auto edgeCompSize32 = static_cast<uint32_t>(edgeCompSize);
    auto edgeRawSize32 = static_cast<uint32_t>(edgeRawSize);

    std::memcpy(output.data() + 0, &nBins16, sizeof(uint16_t));
    output[2] = static_cast<uint8_t>(indexBits);
    std::memcpy(output.data() + 3, &edgeCompSize32, sizeof(uint32_t));
    std::memcpy(output.data() + 7, &edgeRawSize32, sizeof(uint32_t));
    std::memcpy(output.data() + kHeaderSize, edgeCompressed.data(), edgeCompSize);
    std::memcpy(output.data() + kHeaderSize + edgeCompSize,
                packedIndices.data(), packedIndices.size());

    return {
        .data = std::move(output),
        .numFloats = n
    };
}

std::vector<float> QuantileHistogramCompressor::decompress(const CompressedData& compressedData) {
    if (compressedData.numFloats == 0) {
        return {};
    }

    // ── Read header ─────────────────────────────────────────────
    uint16_t nBins;
    uint8_t indexBits;
    uint32_t edgeCompSize, edgeRawSize;

    std::memcpy(&nBins, compressedData.data.data() + 0, sizeof(uint16_t));
    indexBits = compressedData.data[2];
    std::memcpy(&edgeCompSize, compressedData.data.data() + 3, sizeof(uint32_t));
    std::memcpy(&edgeRawSize, compressedData.data.data() + 7, sizeof(uint32_t));

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

    // Reconstruct edges from deltas
    std::vector<float> edges(numEdges);
    edges[0] = edgeDeltas[0];
    for (size_t i = 1; i < numEdges; ++i) {
        edges[i] = edges[i - 1] + edgeDeltas[i];
    }

    // ── Unpack bin indices ──────────────────────────────────────
    const uint8_t* packedData = compressedData.data.data() + kHeaderSize + edgeCompSize;
    auto indices = unpackBits(packedData, compressedData.numFloats, indexBits);

    // ── Reconstruct values as bin midpoints ─────────────────────
    std::vector<float> output(compressedData.numFloats);
    for (size_t i = 0; i < compressedData.numFloats; ++i) {
        uint32_t idx = indices[i];
        output[i] = 0.5f * (edges[idx] + edges[idx + 1]);
    }

    return output;
}

void QuantileHistogramCompressor::configure(const std::map<std::string, std::string>& options) {
    for (const auto& [key, value] : options) {
        if (key == "nBins") {
            _nBins = std::stoi(value);
            if (_nBins < 2 || _nBins > 65536) {
                throw std::invalid_argument("Invalid nBins. Must be between 2 and 65536.");
            }
        } else if (key == "edgeCompressionLevel") {
            _edgeCompressionLevel = std::stoi(value);
            if (_edgeCompressionLevel < 1 || _edgeCompressionLevel > 22) {
                throw std::invalid_argument("Invalid edgeCompressionLevel. Must be between 1 and 22.");
            }
        } else {
            throw std::invalid_argument("Unknown quantile-histogram option: " + key);
        }
    }
}

std::map<std::string, std::string> QuantileHistogramCompressor::getConfig() const {
    return {
        {"nBins", std::to_string(_nBins)},
        {"edgeCompressionLevel", std::to_string(_edgeCompressionLevel)}
    };
}

std::string QuantileHistogramCompressor::name() const {
    return "quantile-histogram";
}

std::string QuantileHistogramCompressor::description() const {
    return "Quantile histogram lossy compressor. Bins data at equal-count quantiles "
           "so bin edges adapt to the data distribution. Edges are delta-coded and "
           "zstd-compressed. Bin indices are bit-packed raw (already at maximum entropy "
           "since all bins have equal occupancy).";
}

std::string QuantileHistogramCompressor::version() const {
    return "1.0.0";
}

std::string QuantileHistogramCompressor::usage() const {
    return "Options:\n"
           "  nBins=<int>                Number of histogram bins (2-65536). Default is 256.\n"
           "  edgeCompressionLevel=<int> Zstd level for bin edges (1-22). Default is 3.";
}