#include <format>

#include <zlib.h>

#include "ZlibCompressor.hpp"

CompressedData ZlibCompressor::compress(const std::vector<float>& data) {
    // Setup
    const uint8_t* inputBytes = reinterpret_cast<const uint8_t*>(data.data());
    const uLongf inputSize = static_cast<uLongf>(data.size() * sizeof(float));
    uLongf maxCompressedSize = compressBound(inputSize);
    std::vector<std::uint8_t> compressedData(maxCompressedSize);

    // Compress
    int res = compress2(
        compressedData.data(),
        &maxCompressedSize,
        inputBytes,
        inputSize,
        _compressionLevel
    );

    // Error checking
    if (res != Z_OK) {
        throw std::runtime_error("Zlib compression failed with error code: " + std::to_string(res));
    }

    // Return resized buffer
    compressedData.resize(maxCompressedSize);
    return {
        .data = std::move(compressedData),
        .numFloats = data.size()
    };
}

std::vector<float> ZlibCompressor::decompress(const CompressedData& compressedData) {
    const std::size_t floatCount = compressedData.numFloats;
    uLongf outputBytes = static_cast<uLongf>(floatCount * sizeof(float));
    std::vector<float> decompressedData(floatCount);

    // Decompress
    int res = uncompress(
        reinterpret_cast<Bytef*>(decompressedData.data()),
        // uncompress will set this to the actual output byte size; initialize with expected
        &outputBytes,
        compressedData.data.data(),
        static_cast<uLongf>(compressedData.data.size())
    );

    // Error checking
    if (res != Z_OK) {
        throw std::runtime_error("Zlib decompression failed with error code: " + std::to_string(res));
    }

    // Return decompressed data
    return decompressedData;
}

void ZlibCompressor::configure(const std::map<std::string, std::string>& options) {
    for (const auto& option : options) {
        if (option.first == "compressionLevel") {
            _compressionLevel = std::stoi(option.second);

            // Validate
            if (_compressionLevel < 0 || _compressionLevel > 9) {
                throw std::runtime_error("Invalid compression level for zlib. Must be between 0 and 9.");
            }
        }
    }
}

std::map<std::string, std::string> ZlibCompressor::getConfig() const {
    return {
        {"compressionLevel", std::to_string(_compressionLevel)}
    };
}

std::string ZlibCompressor::name() const {
    return "zlib";
}

std::string ZlibCompressor::description() const {
    return "Lossless compressor using zlib algorithm.";
}

std::string ZlibCompressor::version() const {
    return std::format("zlib {}", ZLIB_VERSION);
}

std::string ZlibCompressor::usage() const {
    return "Options:\n"
           "  compressionLevel=<int>  Set the zlib compression level (0-9). Default is 6.";
}
