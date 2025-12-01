#include <format>

#include <zlib.h>

#include "ZlibCompressor.hpp"
#include "interface.hpp"

CompressedData ZlibCompressor::compress(const std::vector<float>& data) const {
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
        .bytes = std::move(compressedData),
        .originalSize = data.size() * sizeof(float)
    };
}

std::vector<float> ZlibCompressor::decompress(const CompressedData& compressedData) const {
    // Setup
    if (compressedData.originalSize % sizeof(float) != 0) {
        throw std::runtime_error("CompressedData originalSize is not aligned to sizeof(float).");
    }

    const std::size_t floatCount = compressedData.originalSize / sizeof(float);
    uLongf outputBytes = static_cast<uLongf>(compressedData.originalSize);
    std::vector<float> decompressedData(floatCount);

    // Decompress
    int res = uncompress(
        reinterpret_cast<Bytef*>(decompressedData.data()),
        &outputBytes,
        compressedData.bytes.data(),
        static_cast<uLongf>(compressedData.bytes.size())
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
