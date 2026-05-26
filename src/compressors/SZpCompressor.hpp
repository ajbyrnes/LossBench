/// @file SZpCompressor.hpp
/// @brief Wrapper around the SZp absolute-error-bounded lossy compressor.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

/// @brief Absolute-error-bounded lossy compressor backed by SZp.
///
/// SZp uses OpenMP-parallel block processing; tuning the block size trades
/// parallelism granularity against per-block overhead.
///
/// **CLI options**
/// - `absErrBound` — absolute error bound enforced point-wise. Default 1e-6.
/// - `blockSize`   — block size (number of elements per block). Default 128.
class SZpCompressor : public Compressor {
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
    float _absErrBound = 1e-6f; ///< Point-wise absolute error bound.
    int _blockSize = 128;       ///< Elements per parallel block.
};
