/// @file Compressor.hpp
/// @brief Abstract interface that every LossBench compressor must implement.
///
/// All compression backends in LossBench (lossless, lossy, custom) are
/// accessed through the @ref Compressor interface. The benchmark driver
/// never depends on a specific backend — it constructs an instance via
/// @ref createCompressor (see factory.hpp), configures it with a
/// `key=value` option map parsed from the command line, then calls
/// @ref Compressor::compress / @ref Compressor::decompress on chunks of
/// `std::vector<float>` data extracted from ROOT TTrees.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/// @brief Byte buffer produced by a compressor, plus the float count it represents.
///
/// `numFloats` is stored alongside the compressed bytes so that decompression
/// can pre-size its output buffer without requiring the caller to remember the
/// original chunk size out-of-band.
struct CompressedData {
    /// Compressed payload bytes.
    std::vector<std::uint8_t> data;

    /// Number of `float` values represented by @ref data.
    std::size_t numFloats;
};

/// @brief Pure abstract interface for all LossBench compressors.
///
/// Implementations must provide:
///   - a way to compress `float` data into bytes,
///   - a way to restore the bytes back to `float` data, and
///   - a way to parse implementation-specific configuration options.
///
/// Implementations are expected to be stateful with respect to configuration
/// (set once via @ref configure, then reused across many compress/decompress
/// calls), but @ref compress and @ref decompress themselves should not retain
/// state between calls beyond what is needed to honor the configured options.
class Compressor {
public:
    virtual ~Compressor() = default;

    /// @brief Compress a vector of floats into a byte buffer.
    /// @param data Input samples to compress.
    /// @return Compressed bytes plus the input element count.
    virtual CompressedData compress(const std::vector<float>& data) = 0;

    /// @brief Decompress a byte buffer back into floats.
    /// @param compressedData Buffer previously produced by @ref compress.
    /// @return Reconstructed (possibly lossy) float values.
    virtual std::vector<float> decompress(const CompressedData& compressedData) = 0;

    /// @brief Apply implementation-specific options parsed from the CLI.
    ///
    /// Keys and accepted values are documented per-compressor via
    /// @ref usage. Unknown keys should be reported via an exception so that
    /// misconfigured runs fail loudly rather than silently using defaults.
    virtual void configure(const std::map<std::string, std::string>& options) = 0;

    /// @brief Return the current configuration as a string map.
    ///
    /// The benchmark driver serializes this map into the JSONL results so
    /// each row records exactly which settings produced its metrics.
    virtual std::map<std::string, std::string> getConfig() const = 0;

    /// @brief Short identifier used to select this compressor from the CLI.
    virtual std::string name() const = 0;

    /// @brief Human-readable one-line description of the compressor.
    virtual std::string description() const = 0;

    /// @brief Version string of the underlying compression library, if any.
    virtual std::string version() const = 0;

    /// @brief Multi-line usage / option documentation shown via `--compressor-help`.
    virtual std::string usage() const = 0;

    /// @brief Provide the shape of the next chunk for multi-dimensional backends.
    ///
    /// Called once per chunk before @ref compress / @ref decompress with the
    /// logical dimensions of the data (e.g. `{nRows, rowLen}` for `padded2d`
    /// layouts). The default implementation is a no-op; compressors that
    /// support multi-dimensional inputs natively (ZFP, SZ3, SPERR, ...)
    /// should override this to wire the dimensions into their native API.
    virtual void setDimensions(const std::vector<std::size_t>& /*dims*/) {}
};
