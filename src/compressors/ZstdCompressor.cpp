/// @file ZstdCompressor.cpp
/// @brief Implementation of @ref ZstdCompressor — see the header for option docs.

#include <format>
#include <stdexcept>

#include <zstd.h>

#include "ZstdCompressor.hpp"

CompressedData ZstdCompressor::compress(const std::vector<float>& data) {
    // Setup
    const void* inputBytes = data.data();
    const size_t inputSize = data.size() * sizeof(float);
    size_t maxCompressedSize = ZSTD_compressBound(inputSize);
    std::vector<std::uint8_t> compressedData(maxCompressedSize);

    // Compress
    size_t compressedSize = ZSTD_compress(
        compressedData.data(),
        maxCompressedSize,
        inputBytes,
        inputSize,
        _compressionLevel
    );

    // Error checking
    if (ZSTD_isError(compressedSize)) {
        throw std::runtime_error(std::format("Zstd compression failed: {}", ZSTD_getErrorName(compressedSize)));
    }

    // Return resized buffer
    compressedData.resize(compressedSize);
    return {
        .data = std::move(compressedData),
        .numFloats = data.size()
    };
}

std::vector<float> ZstdCompressor::decompress(const CompressedData& compressedData) {
    const std::size_t floatCount = compressedData.numFloats;
    const size_t outputSize = floatCount * sizeof(float);
    std::vector<float> decompressedData(floatCount);

    // Decompress
    size_t result = ZSTD_decompress(
        decompressedData.data(),
        outputSize,
        compressedData.data.data(),
        compressedData.data.size()
    );

    // Error checking
    if (ZSTD_isError(result)) {
        throw std::runtime_error(std::format("Zstd decompression failed: {}", ZSTD_getErrorName(result)));
    }

    return decompressedData;
}

void ZstdCompressor::configure(const std::map<std::string, std::string>& options) {
    for (const auto& [key, value] : options) {
        if (key == "compressionLevel") {
            _compressionLevel = std::stoi(value);

            // Validate (zstd supports 1-22, with negative levels for fast compression)
            if (_compressionLevel < ZSTD_minCLevel() || _compressionLevel > ZSTD_maxCLevel()) {
                throw std::invalid_argument(std::format(
                    "Invalid compression level for zstd. Must be between {} and {}.",
                    ZSTD_minCLevel(), ZSTD_maxCLevel()
                ));
            }
        } else {
            throw std::invalid_argument("Unknown zstd option: " + key);
        }
    }
}

std::map<std::string, std::string> ZstdCompressor::getConfig() const {
    return {
        {"compressionLevel", std::to_string(_compressionLevel)}
    };
}

std::string ZstdCompressor::name() const {
    return "zstd";
}

std::string ZstdCompressor::description() const {
    return "Lossless compressor using Zstandard algorithm.";
}

std::string ZstdCompressor::version() const {
    return std::format("zstd {}", ZSTD_versionString());
}

std::string ZstdCompressor::usage() const {
    return std::format(
        "Options:\n"
        "  compressionLevel=<int>  Set the zstd compression level ({}-{}). Default is 3.",
        ZSTD_minCLevel(), ZSTD_maxCLevel()
    );
}
