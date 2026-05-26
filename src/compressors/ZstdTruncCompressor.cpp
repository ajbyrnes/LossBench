/// @file ZstdTruncCompressor.cpp
/// @brief Implementation of @ref ZstdTruncCompressor — see the header for option docs.

#include <bit>
#include <cstdint>
#include <format>
#include <stdexcept>

#include <zstd.h>

#include "ZstdTruncCompressor.hpp"

static std::vector<float> truncateMantissa(const std::vector<float>& data, int truncBits) {
    if (truncBits == 0) {
        return data;
    }

    const uint32_t mask = ~((1u << truncBits) - 1);
    std::vector<float> truncated(data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        uint32_t bits = std::bit_cast<uint32_t>(data[i]);
        bits &= mask;
        truncated[i] = std::bit_cast<float>(bits);
    }

    return truncated;
}

CompressedData ZstdTruncCompressor::compress(const std::vector<float>& data) {
    // Truncate mantissa bits
    std::vector<float> truncated = truncateMantissa(data, _truncBits);

    // Compress with zstd
    const void* inputBytes = truncated.data();
    const size_t inputSize = truncated.size() * sizeof(float);
    size_t maxCompressedSize = ZSTD_compressBound(inputSize);
    std::vector<std::uint8_t> compressedData(maxCompressedSize);

    size_t compressedSize = ZSTD_compress(
        compressedData.data(),
        maxCompressedSize,
        inputBytes,
        inputSize,
        _compressionLevel
    );

    if (ZSTD_isError(compressedSize)) {
        throw std::runtime_error(std::format("Zstd compression failed: {}", ZSTD_getErrorName(compressedSize)));
    }

    compressedData.resize(compressedSize);
    return {
        .data = std::move(compressedData),
        .numFloats = data.size()
    };
}

std::vector<float> ZstdTruncCompressor::decompress(const CompressedData& compressedData) {
    const std::size_t floatCount = compressedData.numFloats;
    const size_t outputSize = floatCount * sizeof(float);
    std::vector<float> decompressedData(floatCount);

    size_t result = ZSTD_decompress(
        decompressedData.data(),
        outputSize,
        compressedData.data.data(),
        compressedData.data.size()
    );

    if (ZSTD_isError(result)) {
        throw std::runtime_error(std::format("Zstd decompression failed: {}", ZSTD_getErrorName(result)));
    }

    return decompressedData;
}

void ZstdTruncCompressor::configure(const std::map<std::string, std::string>& options) {
    for (const auto& [key, value] : options) {
        if (key == "compressionLevel") {
            _compressionLevel = std::stoi(value);
            if (_compressionLevel < ZSTD_minCLevel() || _compressionLevel > ZSTD_maxCLevel()) {
                throw std::invalid_argument(std::format(
                    "Invalid compression level. Must be between {} and {}.",
                    ZSTD_minCLevel(), ZSTD_maxCLevel()
                ));
            }
        } else if (key == "truncBits") {
            _truncBits = std::stoi(value);
            if (_truncBits < 0 || _truncBits > 23) {
                throw std::invalid_argument("Invalid truncBits. Must be between 0 and 23.");
            }
        } else {
            throw std::invalid_argument("Unknown zstd-trunc option: " + key);
        }
    }
}

std::map<std::string, std::string> ZstdTruncCompressor::getConfig() const {
    return {
        {"compressionLevel", std::to_string(_compressionLevel)},
        {"truncBits", std::to_string(_truncBits)}
    };
}

std::string ZstdTruncCompressor::name() const {
    return "zstd-trunc";
}

std::string ZstdTruncCompressor::description() const {
    return "Lossy compressor that truncates mantissa bits before zstd compression.";
}

std::string ZstdTruncCompressor::version() const {
    return std::format("zstd {}", ZSTD_versionString());
}

std::string ZstdTruncCompressor::usage() const {
    return std::format(
        "Options:\n"
        "  compressionLevel=<int>  Zstd compression level ({}-{}). Default is 3.\n"
        "  truncBits=<int>         Number of mantissa bits to truncate (0-23). Default is 0.",
        ZSTD_minCLevel(), ZSTD_maxCLevel()
    );
}
