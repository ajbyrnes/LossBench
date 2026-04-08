#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

class ZFPXCompressor : public Compressor {
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
    // Compression mode: "rate", "precision", "accuracy", "reversible"
    std::string _mode = "accuracy";

    // Mode-specific parameters
    double _rate = 8.0;          // bits per value (for fixed-rate mode)
    unsigned int _precision = 16; // bit planes (for fixed-precision mode)
    double _tolerance = 1e-6;    // absolute error tolerance (for fixed-accuracy mode)
};
