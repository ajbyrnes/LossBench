#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct CompressedData{
    std::vector<std::uint8_t> bytes;
    size_t originalSize;
};

// Pure abstract interface for compressors. 
// Implementations must provide:
//     a way to compress float data to bytes, 
//     a way to restore compressed byte data to floats
//     a way to parse configuration args.
class Compressor {
public:
    virtual ~Compressor() = default;

    // Compress a vector of floats into a byte buffer.
    virtual CompressedData compress(const std::vector<float>& data) const = 0;

    // Decompress a byte buffer back into floats.
    virtual std::vector<float> decompress(const CompressedData& compressedData) const = 0;

    // Parse comma-separated arguments specific to the compressor implementation.
    virtual void configure(const std::map<std::string, std::string>& options) = 0;

    // Compressor name.
    virtual std::string name() const = 0;

    // Compressor description string.
    virtual std::string description() const = 0;

    // Compressor version string.
    virtual std::string version() const = 0;

    // Compressor usage string.
    virtual std::string usage() const = 0;
};