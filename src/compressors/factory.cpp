#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ZstdCompressor.hpp"
#include "ZstdTruncCompressor.hpp"
#ifdef HAS_SZ3
#include "SZ3Compressor.hpp"
#endif
#ifdef HAS_ZFP
#include "ZFPCompressor.hpp"
#endif
#ifdef HAS_SPERR
#include "SPERRCompressor.hpp"
#endif
#ifdef HAS_SZP
#include "SZpCompressor.hpp"
#endif
#ifdef HAS_MGARD
#include "MGARDCompressor.hpp"
#endif
#ifdef HAS_ZFPX
#include "ZFPXCompressor.hpp"
#endif
#include "factory.hpp"

using CompressorFactory = std::unique_ptr<Compressor>(*)();

static const std::unordered_map<std::string, CompressorFactory> factories = {
    {"zstd", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<ZstdCompressor>(); }},
    {"zstd-trunc", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<ZstdTruncCompressor>(); }},
#ifdef HAS_SZ3
    {"sz3", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<SZ3Compressor>(); }},
#endif
#ifdef HAS_ZFP
    {"zfp", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<ZFPCompressor>(); }},
#endif
#ifdef HAS_SPERR
    {"sperr", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<SPERRCompressor>(); }},
#endif
#ifdef HAS_SZP
    {"szp", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<SZpCompressor>(); }},
#endif
#ifdef HAS_MGARD
    {"mgard", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<MGARDCompressor>(); }},
#endif
#ifdef HAS_ZFPX
    {"zfpx", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<ZFPXCompressor>(); }},
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
