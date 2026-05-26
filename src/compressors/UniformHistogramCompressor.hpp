/// @file UniformHistogramCompressor.hpp
/// @brief Custom lossy compressor: uniform-width histogram quantization + zstd.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

/// @brief Lossy compressor that quantizes each value to a uniform-width
/// histogram bin and then losslessly compresses the bin indices with zstd.
///
/// Useful as a simple baseline for comparing histogram-style schemes
/// (@ref QuantileHistogramCompressor, @ref QuantileResidualCompressor)
/// against an equal-width binning of the input range.
///
/// **CLI options**
/// - `nBins`     — number of histogram bins (2 … 65536). Default 256.
/// - `zstdLevel` — zstd level for the index stream (1 … 22). Default 3.
class UniformHistogramCompressor : public Compressor {
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
    int _nBins = 256;   ///< Number of equal-width histogram bins (2–65536).
    int _zstdLevel = 3; ///< zstd level used to compress bin indices (1–22).
};
