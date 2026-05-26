/// @file TuckerCompressor.hpp
/// @brief Tucker-decomposition-based lossy compressor.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

/// @brief Lossy compressor based on truncated Tucker tensor decomposition.
///
/// **CLI options**
/// - `epsilon` — relative truncation tolerance. Default 0.1.
class TuckerCompressor : public Compressor {
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
    double _epsilon = 0.1; ///< Relative truncation tolerance.
};
