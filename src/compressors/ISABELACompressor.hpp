/// @file ISABELACompressor.hpp
/// @brief Wrapper around the ISABELA windowed B-spline / wavelet compressor.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

/// @brief Lossy compressor backed by ISABELA.
///
/// ISABELA processes the input in fixed-size windows, sorts each window,
/// fits either B-splines or wavelets to the sorted values, and stores the
/// coefficients plus an optional bounded residual.
///
/// **CLI options**
/// - `windowSize`    — elements per window. Default 1024.
/// - `ncoefficients` — coefficients kept per window. Default 30.
/// - `errorRate`     — max relative residual (0 disables residual coding). Default 0.01.
/// - `transform`     — 0 = B-splines, 1 = wavelets. Default 0.
class ISABELACompressor : public Compressor {
public:
    CompressedData compress(const std::vector<float>& data) override;
    std::vector<float> decompress(const CompressedData& compressedData) override;
    void configure(const std::map<std::string, std::string>& options) override;
    std::map<std::string, std::string> getConfig() const override;
    std::string name() const override;
    std::string description() const override;
    std::string version() const override;
    std::string usage() const override;

private:
    std::uint32_t _windowSize = 1024;    ///< Elements per processing window.
    std::uint32_t _ncoefficients = 30;   ///< B-spline / wavelet coefficients kept per window.
    float _errorRate = 0.01f;            ///< Max relative residual; 0 disables residual encoding.
    int _transform = 0;                  ///< 0 = B-splines, 1 = wavelets.
};
