#include "ZFPXCompressor.hpp"
#include <zfp.h>
#include <stdexcept>

CompressedData ZFPXCompressor::compress(const std::vector<float>& data) {
    zfp_field* field = zfp_field_1d(
        const_cast<float*>(data.data()),
        zfp_type_float,
        data.size()
    );

    if (!field) {
        throw std::runtime_error("ZFP-X: failed to create field");
    }

    zfp_stream* zfp = zfp_stream_open(nullptr);
    if (!zfp) {
        zfp_field_free(field);
        throw std::runtime_error("ZFP-X: failed to open stream");
    }

    if (_mode == "rate") {
        zfp_stream_set_rate(zfp, _rate, zfp_type_float, 1, zfp_false);
    } else if (_mode == "precision") {
        zfp_stream_set_precision(zfp, _precision);
    } else if (_mode == "accuracy") {
        zfp_stream_set_accuracy(zfp, _tolerance);
    } else if (_mode == "reversible") {
        zfp_stream_set_reversible(zfp);
    } else {
        zfp_stream_close(zfp);
        zfp_field_free(field);
        throw std::runtime_error("ZFP-X: unknown mode: " + _mode);
    }

    size_t bufsize = zfp_stream_maximum_size(zfp, field);
    std::vector<uint8_t> buffer(bufsize);

    bitstream* stream = stream_open(buffer.data(), bufsize);
    if (!stream) {
        zfp_stream_close(zfp);
        zfp_field_free(field);
        throw std::runtime_error("ZFP-X: failed to open bitstream");
    }
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    size_t compressedSize = zfp_compress(zfp, field);
    if (compressedSize == 0) {
        stream_close(stream);
        zfp_stream_close(zfp);
        zfp_field_free(field);
        throw std::runtime_error("ZFP-X: compression failed");
    }

    buffer.resize(compressedSize);

    stream_close(stream);
    zfp_stream_close(zfp);
    zfp_field_free(field);

    return {
        .data = std::move(buffer),
        .numFloats = data.size()
    };
}

std::vector<float> ZFPXCompressor::decompress(const CompressedData& compressedData) {
    std::vector<float> output(compressedData.numFloats);

    zfp_field* field = zfp_field_1d(
        output.data(),
        zfp_type_float,
        compressedData.numFloats
    );

    if (!field) {
        throw std::runtime_error("ZFP-X: failed to create field for decompression");
    }

    zfp_stream* zfp = zfp_stream_open(nullptr);
    if (!zfp) {
        zfp_field_free(field);
        throw std::runtime_error("ZFP-X: failed to open stream for decompression");
    }

    if (_mode == "rate") {
        zfp_stream_set_rate(zfp, _rate, zfp_type_float, 1, zfp_false);
    } else if (_mode == "precision") {
        zfp_stream_set_precision(zfp, _precision);
    } else if (_mode == "accuracy") {
        zfp_stream_set_accuracy(zfp, _tolerance);
    } else if (_mode == "reversible") {
        zfp_stream_set_reversible(zfp);
    }

    bitstream* stream = stream_open(
        const_cast<uint8_t*>(compressedData.data.data()),
        compressedData.data.size()
    );
    if (!stream) {
        zfp_stream_close(zfp);
        zfp_field_free(field);
        throw std::runtime_error("ZFP-X: failed to open bitstream for decompression");
    }
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    size_t result = zfp_decompress(zfp, field);
    if (result == 0) {
        stream_close(stream);
        zfp_stream_close(zfp);
        zfp_field_free(field);
        throw std::runtime_error("ZFP-X: decompression failed");
    }

    stream_close(stream);
    zfp_stream_close(zfp);
    zfp_field_free(field);

    return output;
}

void ZFPXCompressor::configure(const std::map<std::string, std::string>& options) {
    _mode = "accuracy";
    _rate = 8.0;
    _precision = 16;
    _tolerance = 1e-6;

    for (const auto& [key, value] : options) {
        if (key == "mode") {
            if (value != "rate" && value != "precision" && value != "accuracy" && value != "reversible") {
                throw std::invalid_argument("Invalid ZFP-X mode: " + value +
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
            throw std::invalid_argument("Unknown ZFP-X option: " + key);
        }
    }
}

std::map<std::string, std::string> ZFPXCompressor::getConfig() const {
    std::map<std::string, std::string> configMap;

    configMap["mode"] = _mode;
    configMap["rate"] = std::to_string(_rate);
    configMap["precision"] = std::to_string(_precision);
    configMap["tolerance"] = std::to_string(_tolerance);

    return configMap;
}

std::string ZFPXCompressor::name() const {
    return "ZFP-X";
}

std::string ZFPXCompressor::description() const {
    return "Lossy floating-point compression using ZFP-X";
}

std::string ZFPXCompressor::version() const {
    return zfp_version_string;
}

std::string ZFPXCompressor::usage() const {
    return "ZFP-X compressor options:\n"
           "  - mode:       string, Compression mode (default: accuracy)\n"
           "                Modes: rate, precision, accuracy, reversible\n"
           "  - rate:       double, Bits per value for fixed-rate mode (default: 8.0)\n"
           "  - precision:  uint, Bit planes for fixed-precision mode (default: 16)\n"
           "  - tolerance:  double, Absolute error tolerance for fixed-accuracy mode (default: 1e-6)\n"
           "\n"
           "Examples:\n"
           "  zfpx:mode=accuracy,tolerance=0.001\n"
           "  zfpx:mode=rate,rate=4.0\n"
           "  zfpx:mode=precision,precision=11\n"
           "  zfpx:mode=reversible";
}
