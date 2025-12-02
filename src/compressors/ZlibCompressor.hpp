#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

class ZlibCompressor : public Compressor {
public:
    CompressedData compress(const std::vector<float>& data) const override;
    std::vector<float> decompress(const CompressedData& compressedData) const override;
    void configure(const std::map<std::string, std::string>& options) override;
    std::string name() const override;
    std::string description() const override;
    std::string version() const override;
    std::string usage() const override;

private:
    int _compressionLevel = 6; // Default zlib compression level
};
