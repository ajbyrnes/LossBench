#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ZlibCompressor.hpp"
#include "ZlibTruncCompressor.hpp"
#ifdef HAS_SZ3
#include "SZ3Compressor.hpp"
#endif
#include "factory.hpp"

using CompressorFactory = std::unique_ptr<Compressor>(*)();

static const std::unordered_map<std::string, CompressorFactory> factories = {
    {"zlib", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<ZlibCompressor>(); }},
    {"zlib-trunc", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<ZlibTruncCompressor>(); }},
#ifdef HAS_SZ3
    {"sz3", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<SZ3Compressor>(); }},
#endif
};

std::unique_ptr<Compressor> createCompressor(const std::string& name) {
    const auto it = factories.find(name);
    if (it != factories.end()) {
        return it->second();
    }

    throw std::invalid_argument("Unknown compressor: " + name);
}

std::vector<std::string> getAvailableCompressors() {
    std::vector<std::string> names;
    names.reserve(factories.size());
    for (const auto& [name, _] : factories) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}
