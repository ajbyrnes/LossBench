/// @file QuantileHistogramCompressor.hpp
/// @brief Custom lossy compressor: quantile-spaced histogram + zstd.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

/// @brief Lossy compressor that quantizes values into bins whose edges are
/// data-driven quantiles, then losslessly compresses the bin indices and
/// edges with zstd.
///
/// Compared to @ref UniformHistogramCompressor, the quantile binning keeps
/// roughly equal counts per bin, which usually yields lower mean error on
/// skewed HEP distributions at the cost of storing the bin edges.
///
/// **CLI options**
/// - `nBins`                 — number of quantile bins (2 … 65536). Default 256.
/// - `edgeCompressionLevel`  — zstd level for the edge table (1 … 22). Default 3.
class QuantileHistogramCompressor : public Compressor {
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
    int _nBins = 256;              ///< Number of quantile-spaced bins (2–65536).
    int _edgeCompressionLevel = 3; ///< zstd level for the bin-edge table (1–22).
};
