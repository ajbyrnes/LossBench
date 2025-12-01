#include <format>

#include <zlib.h>

#include "ZlibCompressor.hpp"
#include "interface.hpp"

std::vector<std::uint8_t> ZlibCompressor::compress(const std::vector<float>& data) const {
    // Implement zlib compression logic here
    // Placeholder implementation
    return std::vector<std::uint8_t>{};
}

std::vector<float> ZlibCompressor::decompress(const std::vector<std::uint8_t>& bytes) const {
    // Implement zlib decompression logic here
    // Placeholder implementation
    return std::vector<float>{};
}

void ZlibCompressor::configure(const std::map<std::string, std::string>& options) {
    auto it = options.find("compressionLevel");
    if (it != options.end()) {
        compressionLevel = std::stoi(it->second);
    }
}

std::string ZlibCompressor::name() const {
    return "zlib";
}

std::string ZlibCompressor::description() const {
    return "Lossless compressor using zlib algorithm.";
}

std::string ZlibCompressor::version() const {
    return std::format("zlib {}", ZLIB_VERSION);
}

std::string ZlibCompressor::usage() const {
    return "Options:\n"
           "  compressionLevel=<int>  Set the zlib compression level (0-9). Default is 6.";
}
