#pragma once

#include <chrono>
#include <vector>

#include "Compressor.hpp"

struct BenchmarkResult {
    std::size_t originalSizeBytes;
    std::size_t compressedSizeBytes;

    float compressionRatio;
    float compressionThroughputMbps;
    float decompressionThroughputMbps;

    float absErrorMax;
    float absErrorAvg;
    float relErrorMax;
    float relErrorAvg;

    float MSE;
    float PSNR;

    // Distribution-based metrics (two-sample KS test)
    float ksStatistic;              // Kolmogorov-Smirnov test statistic
    float ksPValue;                 // KS test p-value
    float wassersteinDistance;      // Earth Mover's Distance (Wasserstein-1)

    // Quantile shifts (absolute difference between original and decompressed quantiles)
    float q5Shift;
    float q50Shift;
    float q95Shift;
    float q99Shift;

    std::vector<float> decompressedData;
};

// Run a chunked benchmark: compress/decompress data in chunkSizeBytes segments
// and aggregate metrics across all chunks.
// If chunkSizeBytes is 0 or >= total data size, processes as a single chunk.
// If iterations > 1, runs the benchmark multiple times and averages timing results.
BenchmarkResult runChunkedBenchmark(
    Compressor& compressor,
    const std::vector<float>& data,
    std::size_t chunkSizeBytes,
    std::size_t iterations = 1);