
#pragma once

#include <map>
#include <string>
#include <vector>
// #include <cmath>

#include "Compressor.hpp"
#include <SZ3/utils/Config.hpp>

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
private:
    // _userConfig stores user-specified settings; per-call copies are made in compress/decompress
    SZ3::Config _userConfig;
};
