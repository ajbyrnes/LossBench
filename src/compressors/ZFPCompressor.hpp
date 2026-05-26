/// @file ZFPCompressor.hpp
/// @brief Wrapper around the ZFP floating-point compression library.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

/// @brief Lossy floating-point compressor backed by ZFP.
///
/// ZFP partitions the input into small blocks and encodes them with a
/// transform-based scheme. Four operating modes are exposed; only the
/// parameter relevant to the active mode is consulted.
///
/// **CLI options**
/// - `mode`       — `rate` | `precision` | `accuracy` | `reversible`. Default `accuracy`.
/// - `rate`       — bits per value (fixed-rate mode). Default 8.
/// - `precision`  — bit planes retained (fixed-precision mode). Default 16.
/// - `tolerance`  — absolute error bound (fixed-accuracy mode). Default 1e-6.
///
/// Multi-dimensional inputs (1-D / 2-D / 3-D) are supported via
/// @ref setDimensions and use the matching native ZFP API.
class ZFPCompressor : public Compressor {
public:
    CompressedData compress(const std::vector<float>& data) override;
    std::vector<float> decompress(const CompressedData& compressedData) override;
    void configure(const std::map<std::string, std::string>& options) override;
    std::map<std::string, std::string> getConfig() const override;
    std::string name() const override;
    std::string description() const override;
    std::string version() const override;
    std::string usage() const override;
    void setDimensions(const std::vector<std::size_t>& dims) override;

private:
    std::vector<std::size_t> _dims;  ///< Logical chunk shape; empty means 1-D.
    std::string _mode = "accuracy";  ///< One of "rate" | "precision" | "accuracy" | "reversible".

    double _rate = 8.0;              ///< Bits per value (fixed-rate mode).
    unsigned int _precision = 16;    ///< Bit planes retained (fixed-precision mode).
    double _tolerance = 1e-6;        ///< Absolute error tolerance (fixed-accuracy mode).
};
