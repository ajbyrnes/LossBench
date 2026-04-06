#include "SPERRCompressor.hpp"
#include <SPERR_C_API.h>
#include <SperrConfig.h>
#include <stdexcept>
#include <cstdlib>

CompressedData SPERRCompressor::compress(const std::vector<float>& data) {
    void* dst = nullptr;
    size_t dst_len = 0;

    // Use 2D API with dimy=1 to treat 1D data as a single row
    // Include header so decompression knows dimensions
    int ret = C_API::sperr_comp_2d(
        data.data(),        // src
        1,                  // is_float = true
        data.size(),        // dimx
        1,                  // dimy = 1 for 1D data
        _mode,              // mode: 1=BPP, 2=PSNR, 3=PWE
        _quality,           // quality
        1,                  // out_inc_header = yes
        &dst,               // output buffer (allocated by SPERR)
        &dst_len            // output length
    );

    if (ret != 0) {
        if (dst) free(dst);
        throw std::runtime_error("SPERR compression failed with error code: " + std::to_string(ret));
    }

    // Convert to vector<uint8_t>
    std::vector<uint8_t> compressedData(
        static_cast<uint8_t*>(dst),
        static_cast<uint8_t*>(dst) + dst_len
    );

    // Free SPERR allocated memory
    free(dst);

    return {
        .data = std::move(compressedData),
        .numFloats = data.size()
    };
}

std::vector<float> SPERRCompressor::decompress(const CompressedData& compressedData) {
    // Parse header to get dimensions
    size_t dimx = 0, dimy = 0, dimz = 0;
    int is_float = 0;
    C_API::sperr_parse_header(compressedData.data.data(), &dimx, &dimy, &dimz, &is_float);

    // Strip the 10-byte header for decompression
    const void* bitstream = compressedData.data.data() + 10;
    size_t bitstream_len = compressedData.data.size() - 10;

    void* dst = nullptr;

    int ret = C_API::sperr_decomp_2d(
        bitstream,          // src (without header)
        bitstream_len,      // src_len
        1,                  // output_float = true
        dimx,               // dimx
        dimy,               // dimy
        &dst                // output buffer (allocated by SPERR)
    );

    if (ret != 0) {
        if (dst) free(dst);
        throw std::runtime_error("SPERR decompression failed with error code: " + std::to_string(ret));
    }

    // Convert to vector<float>
    float* floatPtr = static_cast<float*>(dst);
    std::vector<float> decompressedData(floatPtr, floatPtr + (dimx * dimy));

    // Free SPERR allocated memory
    free(dst);

    return decompressedData;
}

void SPERRCompressor::configure(const std::map<std::string, std::string>& options) {
    // Reset to defaults
    _mode = 3;
    _quality = 1e-6;
    _nthreads = 0;

    for (const auto& [key, value] : options) {
        if (key == "mode") {
            if (value == "bpp" || value == "BPP") {
                _mode = 1;
            } else if (value == "psnr" || value == "PSNR") {
                _mode = 2;
            } else if (value == "pwe" || value == "PWE") {
                _mode = 3;
            } else {
                // Allow numeric mode
                int m = std::stoi(value);
                if (m < 1 || m > 3) {
                    throw std::invalid_argument("Invalid SPERR mode: " + value +
                        ". Must be bpp/1, psnr/2, or pwe/3.");
                }
                _mode = m;
            }
        } else if (key == "quality") {
            _quality = std::stod(value);
            if (_quality <= 0) {
                throw std::invalid_argument("Invalid quality value: " + value + ". Must be positive.");
            }
        } else if (key == "bpp") {
            _mode = 1;
            _quality = std::stod(value);
            if (_quality <= 0) {
                throw std::invalid_argument("Invalid bpp value: " + value + ". Must be positive.");
            }
        } else if (key == "psnr") {
            _mode = 2;
            _quality = std::stod(value);
            if (_quality <= 0) {
                throw std::invalid_argument("Invalid psnr value: " + value + ". Must be positive.");
            }
        } else if (key == "pwe" || key == "tolerance") {
            _mode = 3;
            _quality = std::stod(value);
            if (_quality < 0) {
                throw std::invalid_argument("Invalid pwe/tolerance value: " + value + ". Must be non-negative.");
            }
        } else if (key == "nthreads") {
            _nthreads = std::stoul(value);
        } else {
            throw std::invalid_argument("Unknown SPERR option: " + key);
        }
    }
}

std::map<std::string, std::string> SPERRCompressor::getConfig() const {
    std::map<std::string, std::string> configMap;

    std::string modeName;
    switch (_mode) {
        case 1: modeName = "bpp"; break;
        case 2: modeName = "psnr"; break;
        case 3: modeName = "pwe"; break;
        default: modeName = std::to_string(_mode);
    }

    configMap["mode"] = modeName;
    configMap["quality"] = std::to_string(_quality);
    configMap["nthreads"] = std::to_string(_nthreads);

    return configMap;
}

std::string SPERRCompressor::name() const {
    return "SPERR";
}

std::string SPERRCompressor::description() const {
    return "Wavelet-based lossy compression using SPERR";
}

std::string SPERRCompressor::version() const {
    return std::to_string(SPERR_VERSION_MAJOR) + "." +
           std::to_string(SPERR_VERSION_MINOR) + "." +
           std::to_string(SPERR_VERSION_PATCH);
}

std::string SPERRCompressor::usage() const {
    return "SPERR compressor options:\n"
           "  - mode:      string, Compression mode (default: pwe)\n"
           "               Modes: bpp (bits per pixel), psnr, pwe (point-wise error)\n"
           "  - quality:   double, Quality parameter (meaning depends on mode)\n"
           "               bpp: bits per value, psnr: target PSNR in dB, pwe: max error\n"
           "  - bpp:       double, Shorthand for mode=bpp,quality=<value>\n"
           "  - psnr:      double, Shorthand for mode=psnr,quality=<value>\n"
           "  - pwe:       double, Shorthand for mode=pwe,quality=<value>\n"
           "  - tolerance: double, Alias for pwe\n"
           "  - nthreads:  uint, Number of OpenMP threads (0 = all, default: 0)\n"
           "\n"
           "Examples:\n"
           "  sperr:pwe=0.001\n"
           "  sperr:mode=pwe,quality=0.001\n"
           "  sperr:bpp=4.0\n"
           "  sperr:psnr=50";
}
