#include "SZ3Compressor.hpp"
#include <SZ3/api/sz.hpp>


CompressedData SZ3Compressor::compress(const std::vector<float>& data) {
    // Use a fresh config per call so SZ3 internal mutations don't persist
    SZ3::Config config = _userConfig;

    // Set config dimensions
    std::vector<size_t> dims = {data.size()};
    config.setDims(dims.begin(), dims.end());

    // Compress
    size_t compressedSize = 0;
    char* compressedDataBuffer = SZ_compress(
        config,
        data.data(),
        compressedSize
    );

    if (!compressedDataBuffer) {
        throw std::runtime_error("SZ3 compression failed.");
    }

    // Convert to vector<uint8_t>
    std::vector<uint8_t> compressedData(compressedDataBuffer, compressedDataBuffer + compressedSize);

    // Free SZ3 allocated memory
    free(compressedDataBuffer);

    return {
        .data = std::move(compressedData),
        .numFloats = data.size()
    };
}

std::vector<float> SZ3Compressor::decompress(const CompressedData& compressedData) {
    // Use a fresh config per call so SZ3 internal mutations don't persist
    SZ3::Config config = _userConfig;

    // Set config dimensions
    std::vector<size_t> dims = {compressedData.numFloats};
    config.setDims(dims.begin(), dims.end());

    // Allocate output buffer
    float* decompressedDataBuffer = nullptr;

    // Decompress
    SZ_decompress(
        config,
        reinterpret_cast<const char*>(compressedData.data.data()),
        compressedData.data.size(),
        decompressedDataBuffer
    );

    if (!decompressedDataBuffer) {
        throw std::runtime_error("SZ3 decompression failed.");
    }

    // Convert to vector<float>
    std::vector<float> decompressedData(decompressedDataBuffer, decompressedDataBuffer + compressedData.numFloats);

    // Free SZ3 allocated memory
    free(decompressedDataBuffer);

    return decompressedData;
}

void SZ3Compressor::configure(const std::map<std::string, std::string>& options) {
    _userConfig = SZ3::Config();

    for (const auto& [key, value] : options) {
        if (key == "cmprAlgo") {
            _userConfig.cmprAlgo = static_cast<uint8_t>(std::stoul(value));
            // Validate
            // cmprAlgo can be between 0-6
            if (_userConfig.cmprAlgo > 6 || _userConfig.cmprAlgo < 0) {
                throw std::invalid_argument("Invalid cmprAlgo value: " + value + ". Must be between 0 and 6.");
            }
        } else if (key == "errorBoundMode") {
            _userConfig.errorBoundMode = static_cast<uint8_t>(std::stoul(value));

            // Validate 
            // errorBoundMode can be between 0-5
            if (_userConfig.errorBoundMode > 5 || _userConfig.errorBoundMode < 0) {
                throw std::invalid_argument("Invalid errorBoundMode value: " + value + ". Must be between 0 and 4.");
            }
        } else if (key == "absErrorBound") {
            _userConfig.absErrorBound = std::stod(value);
            
            // Validate
            if (_userConfig.absErrorBound < 0) {
                throw std::invalid_argument("Invalid absErrorBound value: " + value + ". Must be non-negative.");
            }

        } else if (key == "relErrorBound") {
            _userConfig.relErrorBound = std::stod(value);

            // Validate
            if (_userConfig.relErrorBound < 0) {
                throw std::invalid_argument("Invalid relErrorBound value: " + value + ". Must be non-negative.");
            }
        } else if (key == "psnrErrorBound") {
            _userConfig.psnrErrorBound = std::stod(value);

            // Validate
            if (_userConfig.psnrErrorBound < 0) {
                throw std::invalid_argument("Invalid psnrErrorBound value: " + value + ". Must be non-negative.");
            }
        } else if (key == "l2normErrorBound") {
            _userConfig.l2normErrorBound = std::stod(value);

            // Validate
            if (_userConfig.l2normErrorBound < 0) {
                throw std::invalid_argument("Invalid l2normErrorBound value: " + value + ". Must be non-negative.");
            }
        } else if (key == "openmp") {
            _userConfig.openmp = (value == "true" || value == "1");
        } else if (key == "quantbinCnt") {
            _userConfig.quantbinCnt = std::stoi(value);

            // Validate
            if (_userConfig.quantbinCnt <= 0) {
                throw std::invalid_argument("Invalid quantbinCnt value: " + value + ". Must be positive.");
            }
        } else if (key == "blockSize") {
            _userConfig.blockSize = std::stoi(value);

            // Validate
            if (_userConfig.blockSize <= 0) {
                throw std::invalid_argument("Invalid blockSize value: " + value + ". Must be positive.");
            }
        } else if (key == "lorenzo") {
            _userConfig.lorenzo = (value == "true" || value == "1");
        } else if (key == "lorenzo2") {
            _userConfig.lorenzo2 = (value == "true" || value == "1");
        } else if (key == "regression") {
            _userConfig.regression = (value == "true" || value == "1");
        } else if (key == "regression2") {
            _userConfig.regression2 = (value == "true" || value == "1");
        } else if (key == "interpAlgo") {
            _userConfig.interpAlgo = static_cast<uint8_t>(std::stoul(value));

            // Validate
            // interpAlgo can be between 0-1
            if (_userConfig.interpAlgo > 1 || _userConfig.interpAlgo < 0) {
                throw std::invalid_argument("Invalid interpAlgo value: " + value + ". Must be between 0 and 1.");
            }
        } else if (key == "interpDirection") {
            _userConfig.interpDirection = static_cast<uint8_t>(std::stoul(value));

            // Uncertain about which values are allowed
        } else if (key == "interpAnchorStride") {
            _userConfig.interpAnchorStride = std::stoi(value);

            // Uncertain about which values are allowed
        } else if (key == "interpAlpha") {
            _userConfig.interpAlpha = std::stod(value);

            // Uncertain about which values are allowed
        } else if (key == "interpBeta") {
            _userConfig.interpBeta = std::stod(value);

            // Uncertain about which values are allowed
        }
    }

    // _userConfig is ready for use; per-call copies are created in compress/decompress
}

std::map<std::string, std::string> SZ3Compressor::getConfig() const {
    std::map<std::string, std::string> configMap;

    configMap["cmprAlgo"] = std::to_string(_userConfig.cmprAlgo);
    configMap["errorBoundMode"] = std::to_string(_userConfig.errorBoundMode);
    configMap["absErrorBound"] = std::to_string(_userConfig.absErrorBound);
    configMap["relErrorBound"] =  std::to_string(_userConfig.relErrorBound);
    configMap["psnrErrorBound"] = std::to_string(_userConfig.psnrErrorBound);
    configMap["l2normErrorBound"] = std::to_string(_userConfig.l2normErrorBound);
    configMap["openmp"] = _userConfig.openmp ? "true" : "false";
    configMap["quantbinCnt"] = std::to_string(_userConfig.quantbinCnt);
    configMap["blockSize"] = std::to_string(_userConfig.blockSize);
    configMap["lorenzo"] = _userConfig.lorenzo ? "true" : "false";
    configMap["lorenzo2"] = _userConfig.lorenzo2 ? "true" : "false";
    configMap["regression"] = _userConfig.regression ? "true" : "false";
    configMap["regression2"] = _userConfig.regression2 ? "true" : "false";
    configMap["interpAlgo"] = std::to_string(_userConfig.interpAlgo);
    configMap["interpDirection"] = std::to_string(_userConfig.interpDirection);
    configMap["interpAnchorStride"] = std::to_string(_userConfig.interpAnchorStride);
    configMap["interpAlpha"] = std::to_string(_userConfig.interpAlpha);
    configMap["interpBeta"] = std::to_string(_userConfig.interpBeta);

    return configMap;
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
