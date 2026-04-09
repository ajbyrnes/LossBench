#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

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
    // Number of elements per window (default 1024)
    uint32_t _windowSize = 1024;

    // Number of B-spline/wavelet coefficients per window (default 30)
    uint32_t _ncoefficients = 30;

    // Max relative error rate (0 = no error encoding, default 1% = 0.01)
    float _errorRate = 0.01f;

    // Transform type: 0 = BSplines, 1 = Wavelets
    int _transform = 0;
};
