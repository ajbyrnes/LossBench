/// @file QuantileResidualCompressor.hpp
/// @brief Custom lossy compressor: quantile histogram + bounded sub-bin residual.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

/// @brief Lossy compressor that extends @ref QuantileHistogramCompressor
/// with a small fixed-width residual inside each bin.
///
/// After quantile binning, each value is reconstructed as the bin's
/// midpoint plus a `residualBits`-wide offset within the bin. The residual
/// stream is stored alongside the bin indices and edges and is compressed
/// with zstd. Setting `residualBits = 0` recovers the plain quantile
/// histogram behavior.
///
/// **CLI options**
/// - `nBins`                — number of quantile bins (2 … 65536). Default 256.
/// - `residualBits`         — bits per sub-bin residual (0 … 16). Default 4.
/// - `edgeCompressionLevel` — zstd level for the edge table (1 … 22). Default 3.
class QuantileResidualCompressor : public Compressor {
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
    int _nBins = 256;              ///< Number of quantile bins (2–65536).
    int _residualBits = 4;         ///< Bits per sub-bin residual (0–16); 0 disables residuals.
    int _edgeCompressionLevel = 3; ///< zstd level for the bin-edge table (1–22).
};
