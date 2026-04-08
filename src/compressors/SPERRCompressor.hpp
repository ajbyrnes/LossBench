#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

class SPERRCompressor : public Compressor {
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
    // Compression mode: 1=bitrate, 2=PSNR, 3=PWE
    int _mode = 3;  // default: point-wise error (PWE)

    // Quality parameter (meaning depends on mode)
    double _quality = 1e-6;

    // Number of OpenMP threads (0 = use all)
    size_t _nthreads = 0;
};
