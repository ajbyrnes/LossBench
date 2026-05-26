/// @file ZstdTruncCompressor.hpp
/// @brief Bit-truncation pre-processor followed by lossless zstd.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

/// @brief Lossy compressor that zeroes the low mantissa bits of each
/// IEEE-754 float before handing the result to zstd.
///
/// The truncation step trades precision for compressibility: zeroed bits
/// create long runs that zstd encodes very cheaply. With `truncBits = 0`
/// the result is bit-identical to @ref ZstdCompressor.
///
/// **CLI options**
/// - `compressionLevel` — zstd level, 1 … 22. Default 3.
/// - `truncBits`        — number of mantissa bits to zero, 0 … 23. Default 0.
class ZstdTruncCompressor : public Compressor {
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
    int _truncBits = 0;        ///< Mantissa bits to zero before compression (0–23).
};
