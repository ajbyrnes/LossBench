#pragma once

#include <memory>
#include <string>

#include "Compressor.hpp"

// Create a compressor instance by name (e.g., "zlib").
// Throws std::invalid_argument if the name is unknown.
std::unique_ptr<Compressor> createCompressor(const std::string& name);
