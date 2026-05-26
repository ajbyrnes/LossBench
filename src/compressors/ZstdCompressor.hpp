/// @file ZstdCompressor.hpp
/// @brief Lossless zstd compression baseline.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

/// @brief Lossless reference compressor: Facebook's zstd applied directly
/// to the raw float bytes.
///
/// Serves as the lossless baseline that lossy backends are compared
/// against. The only tunable is the standard zstd compression level.
///
/// **CLI options**
/// - `compressionLevel` — zstd level, 1 (fastest) … 22 (smallest). Default 3.
class ZstdCompressor : public Compressor {
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
    int _compressionLevel = 3; ///< zstd compression level (1–22).
};
