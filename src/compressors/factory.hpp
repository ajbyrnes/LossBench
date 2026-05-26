/// @file factory.hpp
/// @brief Compressor lookup by CLI name.
///
/// The factory decouples the benchmark driver from the concrete compressor
/// classes: callers pass the short name supplied on the command line
/// (e.g. `"zstd"`, `"sz3"`, `"quantile-residual"`) and receive a configured
/// @ref Compressor instance. Which backends are actually registered depends
/// on which optional dependencies were found at CMake configure time.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Compressor.hpp"

/// @brief Construct a compressor by its CLI name.
/// @param name Short name as listed by @ref getAvailableCompressors.
/// @return Owning pointer to a freshly constructed, default-configured compressor.
/// @throws std::invalid_argument if @p name is not a registered compressor.
std::unique_ptr<Compressor> createCompressor(const std::string& name);

/// @brief List all compressor names registered in this build.
///
/// The returned list reflects build-time feature flags (HAS_SZ3, HAS_ZFP,
/// HAS_SPERR, ...): a compressor whose dependency was missing at configure
/// time will not appear. The list is returned sorted for stable output.
std::vector<std::string> getAvailableCompressors();
