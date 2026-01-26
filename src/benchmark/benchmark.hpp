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

    std::vector<float> decompressedData;
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

// Run a chunked benchmark: compress/decompress data in chunkSizeBytes segments
// and aggregate metrics across all chunks.
// If chunkSizeBytes is 0 or >= total data size, processes as a single chunk.
BenchmarkResult runChunkedBenchmark(
    Compressor& compressor,
    const std::vector<float>& data,
    std::size_t chunkSizeBytes);