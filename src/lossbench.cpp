#include <format>
#include <iostream>
#include <memory>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "factory.hpp"
#include "benchmark.hpp"

int main() {
    // Perform in-memory compression with zlib

    // Fill data with random floats
    std::vector<float> data = std::vector<float>(100000);
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (auto& val : data) {
        val = dist(rng);
    }

    auto compressor = createCompressor("zlib");

    std::map<std::string, std::string> options = {
        {"compressionLevel", "9"}
    };
    compressor->configure(options);

    // Timed compression
    CompressionResult compResult = timedCompress(*compressor, data);
    std::cout << std::format(
        "Compressed {} bytes into {} bytes in {:.2f} ms.\n",
        data.size() * sizeof(float),
        compResult.compressed.bytes.size(),
        compResult.elapsed.count()
    );

    // Timed decompression
    DecompressionResult decompResult = timedDecompress(*compressor, compResult.compressed);
    std::cout << std::format(
        "Decompressed back to {} bytes in {:.2f} ms.\n",
        decompResult.decompressed.size() * sizeof(float),
        decompResult.elapsed.count()
    );

    // Compute benchmark metrics
    BenchmarkResult metrics = computeBenchmarkMetrics(
        data,
        compResult,
        decompResult
    );

    std::cout << std::format(
        "Compression Ratio: {:.2f}\n"
        "Compression Throughput: {:.2f} MB/s\n"
        "Decompression Throughput: {:.2f} MB/s\n"
        "Max Absolute Error: {:.6f}\n"
        "Avg Absolute Error: {:.6f}\n"
        "Max Relative Error: {:.6f}\n"
        "Avg Relative Error: {:.6f}\n"
        "MSE: {:.6f}\n"
        "PSNR: {:.2f} dB\n",
        metrics.compressionRatio,
        metrics.compressionThroughputMbps,
        metrics.decompressionThroughputMbps,
        metrics.absErrorMax,
        metrics.absErrorAvg,
        metrics.relErrorMax,
        metrics.relErrorAvg,
        metrics.MSE,
        metrics.PSNR
    );


    return 0;
}
