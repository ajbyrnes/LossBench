/// @file SZ3Compressor.hpp
/// @brief Wrapper around the SZ3 error-bounded lossy compression framework.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"
#include <SZ3/utils/Config.hpp>

/// @brief Error-bounded lossy compressor backed by SZ3.
///
/// SZ3 supports several error-bound modes (absolute, relative, point-wise
/// relative, PSNR, etc.) on 1-D up to 4-D data. The user-facing options are
/// forwarded into an SZ3 `Config` object that is copied per call so the
/// configured template stays untouched.
///
/// Multi-dimensional data is supported via @ref setDimensions; with no
/// dimensions set, data is treated as 1-D.
///
/// See SZ3 documentation for the full list of supported error-bound modes
/// and their semantics.
class SZ3Compressor : public Compressor {
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
    /// User-specified SZ3 settings. Per-call copies are made in compress/decompress
    /// so each invocation can stamp in its own element count without mutating the template.
    SZ3::Config _userConfig;
};
