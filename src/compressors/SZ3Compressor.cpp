#include "SZ3Compressor.hpp"
#include <SZ3/api/sz.hpp>


CompressedData SZ3Compressor::compress(const std::vector<float>& data) {
    // Set _config dimensions
    std::vector<size_t> dims = {data.size()};
    _config.setDims(dims.begin(), dims.end());

    size_t compressedSize = 0;
    return {
        .bytes = std::vector<std::uint8_t>{},
        .originalSize = data.size() * sizeof(float)
    };
}

std::vector<float> SZ3Compressor::decompress(const CompressedData& compressedData) {
    return std::vector<float>{};
}

void SZ3Compressor::configure(const std::map<std::string, std::string>& options) {
    _config = SZ3::Config();

    for (const auto& [key, value] : options) {
        if (key == "cmprAlgo") {
            _config.cmprAlgo = static_cast<uint8_t>(std::stoul(value));

            // Validate
            // cmprAlgo can be between 0-6
            if (_config.cmprAlgo > 6 || _config.cmprAlgo < 0) {
                throw std::invalid_argument("Invalid cmprAlgo value: " + value + ". Must be between 0 and 6.");
            }
        } else if (key == "errorBoundMode") {
            _config.errorBoundMode = static_cast<uint8_t>(std::stoul(value));

            // Validate 
            // errorBoundMode can be between 0-5
            if (_config.errorBoundMode > 5 || _config.errorBoundMode < 0) {
                throw std::invalid_argument("Invalid errorBoundMode value: " + value + ". Must be between 0 and 4.");
            }
        } else if (key == "absErrorBound") {
            _config.absErrorBound = std::stod(value);
            
            // Validate
            if (_config.absErrorBound < 0) {
                throw std::invalid_argument("Invalid absErrorBound value: " + value + ". Must be non-negative.");
            }

        } else if (key == "relErrorBound") {
            _config.relErrorBound = std::stod(value);

            // Validate
            if (_config.relErrorBound < 0) {
                throw std::invalid_argument("Invalid relErrorBound value: " + value + ". Must be non-negative.");
            }
        } else if (key == "psnrErrorBound") {
            _config.psnrErrorBound = std::stod(value);

            // Validate
            if (_config.psnrErrorBound < 0) {
                throw std::invalid_argument("Invalid psnrErrorBound value: " + value + ". Must be non-negative.");
            }
        } else if (key == "l2normErrorBound") {
            _config.l2normErrorBound = std::stod(value);

            // Validate
            if (_config.l2normErrorBound < 0) {
                throw std::invalid_argument("Invalid l2normErrorBound value: " + value + ". Must be non-negative.");
            }
        } else if (key == "openmp") {
            _config.openmp = (value == "true" || value == "1");
        } else if (key == "quantbinCnt") {
            _config.quantbinCnt = std::stoi(value);

            // Validate
            if (_config.quantbinCnt <= 0) {
                throw std::invalid_argument("Invalid quantbinCnt value: " + value + ". Must be positive.");
            }
        } else if (key == "blockSize") {
            _config.blockSize = std::stoi(value);

            // Validate
            if (_config.blockSize <= 0) {
                throw std::invalid_argument("Invalid blockSize value: " + value + ". Must be positive.");
            }
        } else if (key == "lorenzo") {
            _config.lorenzo = (value == "true" || value == "1");
        } else if (key == "lorenzo2") {
            _config.lorenzo2 = (value == "true" || value == "1");
        } else if (key == "regression") {
            _config.regression = (value == "true" || value == "1");
        } else if (key == "regression2") {
            _config.regression2 = (value == "true" || value == "1");
        } else if (key == "interpAlgo") {
            _config.interpAlgo = static_cast<uint8_t>(std::stoul(value));

            // Validate
            // interpAlgo can be between 0-1
            if (_config.interpAlgo > 1 || _config.interpAlgo < 0) {
                throw std::invalid_argument("Invalid interpAlgo value: " + value + ". Must be between 0 and 1.");
            }
        } else if (key == "interpDirection") {
            _config.interpDirection = static_cast<uint8_t>(std::stoul(value));

            // Uncertain about which values are allowed
        } else if (key == "interpAnchorStride") {
            _config.interpAnchorStride = std::stoi(value);

            // Uncertain about which values are allowed
        } else if (key == "interpAlpha") {
            _config.interpAlpha = std::stod(value);

            // Uncertain about which values are allowed
        } else if (key == "interpBeta") {
            _config.interpBeta = std::stod(value);

            // Uncertain about which values are allowed
        }
    }

}

std::string SZ3Compressor::name() const {
    return "SZ3";
}

std::string SZ3Compressor::description() const {
    return "Error-bounded lossy compression using SZ3";
}

std::string SZ3Compressor::version() const {
    return "";
}

// Config settings are listed in <SZ3/utils/Config.hpp>
std::string SZ3Compressor::usage() const {
    return "SZ3 compressor options:\n"
           "  Main options:\n"
           "  - cmprAlgo:         uint8_t, Set the compression algorithm\n"
           "  - errorBoundMode:   uint8_t, Set the error bound mode\n"
           "  - absErrorBound:    double, Set the absolute error bound\n"
           "  - relErrorBound:    double, Set the relative error bound\n"
           "  - psnrErrorBound:   double, Set the PSNR error bound\n"
           "  - l2normErrorBound: double, Set the L2 norm error bound\n"
           "  - openmp:           bool, Enable OpenMP parallelization\n"
           "  Algorithm-specific options:\n"
           "  - quantbinCnt:    int, Maximum number of quantization intervals\n"
           "  - blockSize:      int, Block size for processing\n"
           "  - lorenzo:        bool, Use 1st order Lorenzo predictor\n"
           "  - lorenzo2:       bool, Use 2nd order Lorenzo predictor\n"
           "  - regression:     bool, Use 1st order regression\n"
           "  - regression2:    bool, Use 2nd order regression\n"
           "  - interpAlgo:         uint8_t, Set the interpolation algorithm\n"
           "  - interpDirection:    uint8_t, Set the interpolation direction\n"
           "  - interpAnchorStride: int, Set the interpolation anchor stride\n"
           "  - interpAlpha:        double, Interpolation error-bound tuning parameter\n"
           "  - interpBeta:         double, Interpolation error-bound tuning parameter";
};
