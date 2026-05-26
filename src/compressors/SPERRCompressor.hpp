/// @file SPERRCompressor.hpp
/// @brief Wrapper around the SPERR wavelet-based lossy compressor.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

/// @brief Wavelet-based lossy compressor backed by SPERR.
///
/// SPERR supports three target metrics; the interpretation of `quality`
/// depends on @ref _mode:
///   - bitrate (1) — target bits per value.
///   - PSNR    (2) — target peak signal-to-noise ratio (dB).
///   - PWE     (3) — point-wise absolute error bound.
///
/// **CLI options**
/// - `mode`     — `bitrate` | `psnr` | `pwe`. Default `pwe`.
/// - `quality`  — meaning depends on `mode` (see above). Default 1e-6.
/// - `nthreads` — OpenMP thread count (0 = use all available). Default 0.
///
/// Multi-dimensional data (2-D / 3-D) is supported via @ref setDimensions.
class SPERRCompressor : public Compressor {
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
    int _mode = 3;                   ///< 1=bitrate, 2=PSNR, 3=PWE (point-wise error).
    double _quality = 1e-6;          ///< Quality target; meaning depends on @ref _mode.
    std::size_t _nthreads = 0;       ///< OpenMP threads (0 = library default).
};
