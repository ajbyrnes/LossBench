#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "ZlibCompressor.hpp"
#include "SZ3Compressor.hpp"
#include "factory.hpp"

using CompressorFactory = std::unique_ptr<Compressor>(*)();

std::unique_ptr<Compressor> createCompressor(const std::string& name) {
    static const std::unordered_map<std::string, CompressorFactory> kFactories = {
        {"zlib", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<ZlibCompressor>(); }},
        {"sz3", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<SZ3Compressor>(); }},
    };

    const auto it = kFactories.find(name);
    if (it != kFactories.end()) {
        return it->second();
    }

    throw std::invalid_argument("Unknown compressor: " + name);
}
