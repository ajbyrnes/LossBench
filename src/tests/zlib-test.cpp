#include <format>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "ZlibCompressor.hpp"


int main() {
    // Perform in-memory compression with zlib

    // Fill data with random floats
    std::vector<float> data = std::vector<float>(100000);
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (auto& val : data) {
        val = dist(rng);
    }

    ZlibCompressor compressor;
    // Print compressor info
    std::cout << "Compressor: " << compressor.description() << "\n";
    std::cout << "Version: " << compressor.version() << "\n";
    std::cout << "Usage: " << compressor.usage() << "\n";

    // Compress data
    CompressedData compressedData = compressor.compress(data);
    std::cout << "Original size: " << data.size() * sizeof(float) << " bytes\n";
    std::cout << "Compressed size: " << compressedData.data.size() << " bytes\n";

    // Decompress data
    std::vector<float> decompressedData = compressor.decompress(compressedData);
    std::cout << "Decompressed size: " << decompressedData.size() * sizeof(float) << " bytes\n";

    // Print first 10 values of original data
    std::cout << "First 10 values of original data:\n";
    for (size_t i = 0; i < 10 && i < data.size(); ++i) {
        std::cout << data[i] << " ";
    }
    std::cout << "\n";

    // Print first 10 values of decompressed data
    std::cout << "First 10 values of decompressed data:\n";
    for (size_t i = 0; i < 10 && i < decompressedData.size(); ++i) {
        std::cout << decompressedData[i] << " ";
    }
    std::cout << "\n";
    
    return 0;
}
