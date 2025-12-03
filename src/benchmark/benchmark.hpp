#pragma once

#include <chrono>
#include <vector>

#include "Compressor.hpp"

// Result of a timed compression run.
struct CompressionResult {
    CompressedData compressedData;
    std::chrono::duration<double, std::milli> elapsed;
};

// Result of a timed decompression run.
struct DecompressionResult {
    std::vector<float> decompressedData;
    std::chrono::duration<double, std::milli> elapsed;
};

struct BenchmarkResult {
    float compressionRatio;
    float compressionThroughputMbps;
    float decompressionThroughputMbps;

    float absErrorMax;
    float absErrorAvg;
    float relErrorMax;
    float relErrorAvg;

    float MSE;
    float PSNR;
};

// Run compression while measuring wall-clock time.
CompressionResult timedCompress(
    Compressor& compressor,
    const std::vector<float>& data);

// Run decompression while measuring wall-clock time.
DecompressionResult timedDecompress(
    Compressor& compressor,
    const CompressedData& compressedData);

// Compute benchmark metrics given original and decompressed data.
BenchmarkResult computeBenchmarkMetrics(
    const std::vector<float>& original,
    const CompressionResult& compResult,
    const DecompressionResult& decompResult);