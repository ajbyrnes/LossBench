#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>

#include "ZlibCompressor.hpp"
#include "factory.hpp"


std::unique_ptr<Compressor> createCompressor(const std::string& name) {
    if (name == "zlib") {
        return std::make_unique<ZlibCompressor>();
    }

    throw std::invalid_argument("Unknown compressor: " + name);
}
