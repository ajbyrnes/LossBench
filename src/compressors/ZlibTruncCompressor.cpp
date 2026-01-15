#include <bit>
#include <cstdint>
#include <format>

#include <zlib.h>

#include "ZlibTruncCompressor.hpp"

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

CompressedData ZlibTruncCompressor::compress(const std::vector<float>& data) {
    // Truncate mantissa bits
    std::vector<float> truncated = truncateMantissa(data, _truncBits);

    // Compress with zlib
    const uint8_t* inputBytes = reinterpret_cast<const uint8_t*>(truncated.data());
    const uLongf inputSize = static_cast<uLongf>(truncated.size() * sizeof(float));
    uLongf maxCompressedSize = compressBound(inputSize);
    std::vector<std::uint8_t> compressedData(maxCompressedSize);

    int res = compress2(
        compressedData.data(),
        &maxCompressedSize,
        inputBytes,
        inputSize,
        _compressionLevel
    );

    if (res != Z_OK) {
        throw std::runtime_error("Zlib compression failed with error code: " + std::to_string(res));
    }

    compressedData.resize(maxCompressedSize);
    return {
        .data = std::move(compressedData),
        .numFloats = data.size()
    };
}

std::vector<float> ZlibTruncCompressor::decompress(const CompressedData& compressedData) {
    const std::size_t floatCount = compressedData.numFloats;
    uLongf outputBytes = static_cast<uLongf>(floatCount * sizeof(float));
    std::vector<float> decompressedData(floatCount);

    int res = uncompress(
        reinterpret_cast<Bytef*>(decompressedData.data()),
        &outputBytes,
        compressedData.data.data(),
        static_cast<uLongf>(compressedData.data.size())
    );

    if (res != Z_OK) {
        throw std::runtime_error("Zlib decompression failed with error code: " + std::to_string(res));
    }

    return decompressedData;
}

void ZlibTruncCompressor::configure(const std::map<std::string, std::string>& options) {
    for (const auto& [key, value] : options) {
        if (key == "compressionLevel") {
            _compressionLevel = std::stoi(value);
            if (_compressionLevel < 0 || _compressionLevel > 9) {
                throw std::invalid_argument("Invalid compression level. Must be between 0 and 9.");
            }
        } else if (key == "truncBits") {
            _truncBits = std::stoi(value);
            if (_truncBits < 0 || _truncBits > 23) {
                throw std::invalid_argument("Invalid truncBits. Must be between 0 and 23.");
            }
        } else {
            throw std::invalid_argument("Unknown zlib-trunc option: " + key);
        }
    }
}

std::map<std::string, std::string> ZlibTruncCompressor::getConfig() const {
    return {
        {"compressionLevel", std::to_string(_compressionLevel)},
        {"truncBits", std::to_string(_truncBits)}
    };
}

std::string ZlibTruncCompressor::name() const {
    return "zlib-trunc";
}

std::string ZlibTruncCompressor::description() const {
    return "Lossy compressor that truncates mantissa bits before zlib compression.";
}

std::string ZlibTruncCompressor::version() const {
    return std::format("zlib {}", ZLIB_VERSION);
}

std::string ZlibTruncCompressor::usage() const {
    return "Options:\n"
           "  compressionLevel=<int>  Zlib compression level (0-9). Default is 6.\n"
           "  truncBits=<int>         Number of mantissa bits to truncate (0-23). Default is 0.";
}
