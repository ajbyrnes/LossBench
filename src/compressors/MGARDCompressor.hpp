#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

class MGARDCompressor : public Compressor {
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
    // Error bound mode: ABS or REL
    int _mode = 0;  // 0=ABS, 1=REL

    // Error tolerance
    double _tolerance = 1e-6;

    // Smoothness parameter (s)
    double _smoothness = 0.0;
};
