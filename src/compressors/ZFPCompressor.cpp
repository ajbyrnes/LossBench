/// @file ZFPCompressor.cpp
/// @brief Implementation of @ref ZFPCompressor — see the header for option docs.

#include "ZFPCompressor.hpp"
#include <zfp.h>
#include <stdexcept>

void ZFPCompressor::setDimensions(const std::vector<std::size_t>& dims) {
    _dims = dims;
}

CompressedData ZFPCompressor::compress(const std::vector<float>& data) {
    // Create input field based on dimensionality
    zfp_field* field = nullptr;
    uint dims = 1;
    if (_dims.size() == 2 && _dims[0] > 0 && _dims[1] > 0) {
        // 2D: dims = {nRows, rowLen} → ZFP wants (nx, ny)
        field = zfp_field_2d(
            const_cast<float*>(data.data()),
            zfp_type_float,
            _dims[1],  // nx = rowLen
            _dims[0]   // ny = nRows
        );
        dims = 2;
    } else {
        field = zfp_field_1d(
            const_cast<float*>(data.data()),
            zfp_type_float,
            data.size()
        );
    }

    if (!field) {
        throw std::runtime_error("ZFP: failed to create field");
    }

    // Create compressed stream
    zfp_stream* zfp = zfp_stream_open(nullptr);
    if (!zfp) {
        zfp_field_free(field);
        throw std::runtime_error("ZFP: failed to open stream");
    }

    // Set compression mode
    if (_mode == "rate") {
        zfp_stream_set_rate(zfp, _rate, zfp_type_float, dims, zfp_false);
    } else if (_mode == "precision") {
        zfp_stream_set_precision(zfp, _precision);
    } else if (_mode == "accuracy") {
        zfp_stream_set_accuracy(zfp, _tolerance);
    } else if (_mode == "reversible") {
        zfp_stream_set_reversible(zfp);
    } else {
        zfp_stream_close(zfp);
        zfp_field_free(field);
        throw std::runtime_error("ZFP: unknown mode: " + _mode);
    }

    // Allocate buffer for compressed data
    size_t bufsize = zfp_stream_maximum_size(zfp, field);
    std::vector<uint8_t> buffer(bufsize);

    // Associate bit stream with allocated buffer
    bitstream* stream = stream_open(buffer.data(), bufsize);
    if (!stream) {
        zfp_stream_close(zfp);
        zfp_field_free(field);
        throw std::runtime_error("ZFP: failed to open bitstream");
    }
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    // Compress
    size_t compressedSize = zfp_compress(zfp, field);
    if (compressedSize == 0) {
        stream_close(stream);
        zfp_stream_close(zfp);
        zfp_field_free(field);
        throw std::runtime_error("ZFP: compression failed");
    }

    // Resize buffer to actual compressed size
    buffer.resize(compressedSize);

    // Clean up
    stream_close(stream);
    zfp_stream_close(zfp);
    zfp_field_free(field);

    return {
        .data = std::move(buffer),
        .numFloats = data.size()
    };
}

std::vector<float> ZFPCompressor::decompress(const CompressedData& compressedData) {
    // Allocate output buffer
    std::vector<float> output(compressedData.numFloats);

    // Create output field based on dimensionality
    zfp_field* field = nullptr;
    uint dims = 1;
    if (_dims.size() == 2 && _dims[0] > 0 && _dims[1] > 0) {
        field = zfp_field_2d(
            output.data(),
            zfp_type_float,
            _dims[1],  // nx = rowLen
            _dims[0]   // ny = nRows
        );
        dims = 2;
    } else {
        field = zfp_field_1d(
            output.data(),
            zfp_type_float,
            compressedData.numFloats
        );
    }

    if (!field) {
        throw std::runtime_error("ZFP: failed to create field for decompression");
    }

    // Create compressed stream
    zfp_stream* zfp = zfp_stream_open(nullptr);
    if (!zfp) {
        zfp_field_free(field);
        throw std::runtime_error("ZFP: failed to open stream for decompression");
    }

    // Set compression mode (must match compression settings)
    if (_mode == "rate") {
        zfp_stream_set_rate(zfp, _rate, zfp_type_float, dims, zfp_false);
    } else if (_mode == "precision") {
        zfp_stream_set_precision(zfp, _precision);
    } else if (_mode == "accuracy") {
        zfp_stream_set_accuracy(zfp, _tolerance);
    } else if (_mode == "reversible") {
        zfp_stream_set_reversible(zfp);
    }

    // Associate bit stream with compressed data
    bitstream* stream = stream_open(
        const_cast<uint8_t*>(compressedData.data.data()),
        compressedData.data.size()
    );
    if (!stream) {
        zfp_stream_close(zfp);
        zfp_field_free(field);
        throw std::runtime_error("ZFP: failed to open bitstream for decompression");
    }
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    // Decompress
    size_t result = zfp_decompress(zfp, field);
    if (result == 0) {
        stream_close(stream);
        zfp_stream_close(zfp);
        zfp_field_free(field);
        throw std::runtime_error("ZFP: decompression failed");
    }

    // Clean up
    stream_close(stream);
    zfp_stream_close(zfp);
    zfp_field_free(field);

    return output;
}

void ZFPCompressor::configure(const std::map<std::string, std::string>& options) {
    // Reset to defaults
    _mode = "accuracy";
    _rate = 8.0;
    _precision = 16;
    _tolerance = 1e-6;

    for (const auto& [key, value] : options) {
        if (key == "mode") {
            if (value != "rate" && value != "precision" && value != "accuracy" && value != "reversible") {
                throw std::invalid_argument("Invalid ZFP mode: " + value +
                    ". Must be one of: rate, precision, accuracy, reversible");
            }
            _mode = value;
        } else if (key == "rate") {
            _rate = std::stod(value);
            if (_rate <= 0) {
                throw std::invalid_argument("Invalid rate value: " + value + ". Must be positive.");
            }
        } else if (key == "precision") {
            _precision = std::stoul(value);
            if (_precision == 0 || _precision > ZFP_MAX_PREC) {
                throw std::invalid_argument("Invalid precision value: " + value +
                    ". Must be between 1 and " + std::to_string(ZFP_MAX_PREC) + ".");
            }
        } else if (key == "tolerance") {
            _tolerance = std::stod(value);
            if (_tolerance < 0) {
                throw std::invalid_argument("Invalid tolerance value: " + value + ". Must be non-negative.");
            }
        } else {
            throw std::invalid_argument("Unknown ZFP option: " + key);
        }
    }
}

std::map<std::string, std::string> ZFPCompressor::getConfig() const {
    std::map<std::string, std::string> configMap;

    configMap["mode"] = _mode;
    configMap["rate"] = std::to_string(_rate);
    configMap["precision"] = std::to_string(_precision);
    configMap["tolerance"] = std::to_string(_tolerance);

    return configMap;
}

std::string ZFPCompressor::name() const {
    return "ZFP";
}

std::string ZFPCompressor::description() const {
    return "Lossy floating-point compression using ZFP";
}

std::string ZFPCompressor::version() const {
    return zfp_version_string;
}

std::string ZFPCompressor::usage() const {
    return "ZFP compressor options:\n"
           "  - mode:       string, Compression mode (default: accuracy)\n"
           "                Modes: rate, precision, accuracy, reversible\n"
           "  - rate:       double, Bits per value for fixed-rate mode (default: 8.0)\n"
           "  - precision:  uint, Bit planes for fixed-precision mode (default: 16)\n"
           "  - tolerance:  double, Absolute error tolerance for fixed-accuracy mode (default: 1e-6)\n"
           "\n"
           "Examples:\n"
           "  zfp:mode=accuracy,tolerance=0.001\n"
           "  zfp:mode=rate,rate=4.0\n"
           "  zfp:mode=precision,precision=11\n"
           "  zfp:mode=reversible";
}
