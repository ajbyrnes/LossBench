#include "SZ3Compressor.hpp"
#include <SZ3/api/sz.hpp>


CompressedData SZ3Compressor::compress(const std::vector<float>& data) const {
    size_t compressedSize = 0;
    
    return {
        .bytes = std::vector<std::uint8_t>{},
        .originalSize = data.size() * sizeof(float)
    };
}

std::vector<float> SZ3Compressor::decompress(const CompressedData& compressedData) const {
    return std::vector<float>{};
}

void SZ3Compressor::configure(const std::map<std::string, std::string>& options) {
    _config = SZ3::Config();
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
           "  - absErrBound:      double, Set the absolute error bound\n"
           "  - relErrBound:      double, Set the relative error bound\n"
           "  - psnrErrBound:     double, Set the PSNR error bound\n"
           "  - l2normErrorBound: double, Set the L2 norm error bound\n"
           "  - openMP:           bool, Enable OpenMP parallelization\n"
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
