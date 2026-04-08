#include "MGARDCompressor.hpp"
#include <compress_x.hpp>
#include <stdexcept>
#include <cstdlib>

CompressedData MGARDCompressor::compress(const std::vector<float>& data) {
    mgard_x::Config config;

    std::vector<mgard_x::SIZE> shape = {static_cast<mgard_x::SIZE>(data.size())};

    mgard_x::error_bound_type mode = (_mode == 1)
        ? mgard_x::error_bound_type::REL
        : mgard_x::error_bound_type::ABS;

    void* compressedPtr = nullptr;
    size_t compressedSize = 0;

    auto status = mgard_x::compress(
        1,                              // D = 1 dimension
        mgard_x::data_type::Float,      // float data
        shape,
        _tolerance,
        _smoothness,
        mode,
        data.data(),
        compressedPtr,
        compressedSize,
        config,
        false                           // output not pre-allocated
    );

    if (status != mgard_x::compress_status_type::Success) {
        if (compressedPtr) std::free(compressedPtr);
        throw std::runtime_error("MGARD compression failed");
    }

    std::vector<uint8_t> compressedData(
        static_cast<uint8_t*>(compressedPtr),
        static_cast<uint8_t*>(compressedPtr) + compressedSize
    );

    std::free(compressedPtr);

    return {
        .data = std::move(compressedData),
        .numFloats = data.size()
    };
}

std::vector<float> MGARDCompressor::decompress(const CompressedData& compressedData) {
    mgard_x::Config config;

    void* decompressedPtr = nullptr;

    auto status = mgard_x::decompress(
        compressedData.data.data(),
        compressedData.data.size(),
        decompressedPtr,
        config,
        false                           // output not pre-allocated
    );

    if (status != mgard_x::compress_status_type::Success) {
        if (decompressedPtr) std::free(decompressedPtr);
        throw std::runtime_error("MGARD decompression failed");
    }

    float* floatPtr = static_cast<float*>(decompressedPtr);
    std::vector<float> decompressedData(floatPtr, floatPtr + compressedData.numFloats);

    std::free(decompressedPtr);

    return decompressedData;
}

void MGARDCompressor::configure(const std::map<std::string, std::string>& options) {
    // Reset to defaults
    _mode = 0;
    _tolerance = 1e-6;
    _smoothness = 0.0;

    for (const auto& [key, value] : options) {
        if (key == "mode") {
            if (value == "abs" || value == "ABS") {
                _mode = 0;
            } else if (value == "rel" || value == "REL") {
                _mode = 1;
            } else {
                throw std::invalid_argument("Invalid MGARD mode: " + value +
                    ". Must be abs or rel.");
            }
        } else if (key == "tolerance") {
            _tolerance = std::stod(value);
            if (_tolerance <= 0) {
                throw std::invalid_argument("Invalid tolerance value: " + value +
                    ". Must be positive.");
            }
        } else if (key == "smoothness" || key == "s") {
            _smoothness = std::stod(value);
        } else {
            throw std::invalid_argument("Unknown MGARD option: " + key);
        }
    }
}

std::map<std::string, std::string> MGARDCompressor::getConfig() const {
    std::map<std::string, std::string> configMap;
    configMap["mode"] = (_mode == 1) ? "rel" : "abs";
    configMap["tolerance"] = std::to_string(_tolerance);
    configMap["smoothness"] = std::to_string(_smoothness);
    return configMap;
}

std::string MGARDCompressor::name() const {
    return "MGARD";
}

std::string MGARDCompressor::description() const {
    return "MultiGrid Adaptive Reduction of Data (MGARD) lossy compression";
}

std::string MGARDCompressor::version() const {
    return "1.5.2";
}

std::string MGARDCompressor::usage() const {
    return "MGARD compressor options:\n"
           "  - mode:      string, Error bound type (default: abs)\n"
           "               Modes: abs (absolute), rel (relative)\n"
           "  - tolerance: double, Error tolerance (default: 1e-6)\n"
           "  - smoothness: double, Smoothness parameter s (default: 0.0)\n"
           "  - s:         double, Alias for smoothness\n"
           "\n"
           "Examples:\n"
           "  mgard:tolerance=0.001\n"
           "  mgard:mode=rel,tolerance=0.01\n"
           "  mgard:mode=abs,tolerance=1e-4,s=1.0";
}
