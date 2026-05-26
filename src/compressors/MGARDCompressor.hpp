/// @file MGARDCompressor.hpp
/// @brief Wrapper around the MGARD multigrid-based lossy compressor.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

/// @brief Multigrid-based lossy compressor backed by MGARD.
///
/// MGARD bounds error either absolutely or relative to the input norm and
/// exposes a smoothness parameter `s` that selects the function-space norm
/// in which the bound is enforced (s=0 is the L2 norm).
///
/// **CLI options**
/// - `mode`       — `abs` (0) or `rel` (1) error mode. Default `abs`.
/// - `tolerance`  — error tolerance under the chosen mode. Default 1e-6.
/// - `smoothness` — smoothness parameter `s`. Default 0.
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
    int _mode = 0;            ///< Error-bound mode: 0=ABS, 1=REL.
    double _tolerance = 1e-6; ///< Error tolerance under the chosen mode.
    double _smoothness = 0.0; ///< MGARD smoothness parameter `s`.
};
