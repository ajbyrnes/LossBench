#include "SZpCompressor.hpp"
#include <szp_float.h>
#include <szpd_float.h>
#include <szp_defines.h>
#include <stdexcept>
#include <cstdlib>

CompressedData SZpCompressor::compress(const std::vector<float>& data) {
    size_t outSize = 0;

    // SZp API takes non-const float*; the data is not modified.
    unsigned char* dst = szp_float_openmp_threadblock(
        const_cast<float*>(data.data()),
        &outSize,
        _absErrBound,
        data.size(),
        _blockSize
    );

    if (!dst) {
        throw std::runtime_error("SZp compression failed");
    }

    std::vector<uint8_t> compressedData(dst, dst + outSize);
    free(dst);

    return {
        .data = std::move(compressedData),
        .numFloats = data.size()
    };
}

std::vector<float> SZpCompressor::decompress(const CompressedData& compressedData) {
    // SZp prepends sizeof(float) bytes for absErrBound to the compressed output.
    // The low-level decompression function expects data *after* that header.
    unsigned char* cmpBytes = const_cast<unsigned char*>(compressedData.data.data()) + sizeof(float);
    float* dst = szp_float_decompress_openmp_threadblock(
        compressedData.numFloats,
        _absErrBound,
        _blockSize,
        cmpBytes
    );

    if (!dst) {
        throw std::runtime_error("SZp decompression failed");
    }

    std::vector<float> decompressedData(dst, dst + compressedData.numFloats);
    free(dst);

    return decompressedData;
}

void SZpCompressor::configure(const std::map<std::string, std::string>& options) {
    // Reset to defaults
    _absErrBound = 1e-6f;
    _blockSize = 128;

    for (const auto& [key, value] : options) {
        if (key == "absErrBound") {
            _absErrBound = std::stof(value);
            if (_absErrBound < 0) {
                throw std::invalid_argument("Invalid absErrBound: " + value + ". Must be non-negative.");
            }
        } else if (key == "blockSize") {
            _blockSize = std::stoi(value);
            if (_blockSize <= 0) {
                throw std::invalid_argument("Invalid blockSize: " + value + ". Must be positive.");
            }
        } else {
            throw std::invalid_argument("Unknown SZp option: " + key);
        }
    }
}

std::map<std::string, std::string> SZpCompressor::getConfig() const {
    return {
        {"absErrBound", std::to_string(_absErrBound)},
        {"blockSize", std::to_string(_blockSize)}
    };
}

std::string SZpCompressor::name() const {
    return "SZp";
}

std::string SZpCompressor::description() const {
    return "Error-bounded lossy compression using SZp with OpenMP";
}

std::string SZpCompressor::version() const {
    return std::to_string(szp_VER_MAJOR) + "." +
           std::to_string(szp_VER_MINOR) + "." +
           std::to_string(szp_VER_BUILD);
}

std::string SZpCompressor::usage() const {
    return "SZp compressor options:\n"
           "  - absErrBound: float, Absolute error bound (default: 1e-6)\n"
           "  - blockSize:   int,   Block size for compression (default: 128)\n"
           "\n"
           "Examples:\n"
           "  szp:absErrBound=1e-4\n"
           "  szp:absErrBound=0.001,blockSize=256";
}
