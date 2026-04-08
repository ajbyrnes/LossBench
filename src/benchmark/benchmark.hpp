#pragma once

#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Compressor.hpp"

// Everything a metric function needs to compute its result(s).
// Pre-sorted copies are provided so multiple metrics can share them
// without redundant sorting.
struct MetricContext {
    const std::vector<float>& original;
    const std::vector<float>& decompressed;
    const std::vector<float>& sortedOriginal;
    const std::vector<float>& sortedDecompressed;
    std::size_t originalSizeBytes;
    std::size_t compressedSizeBytes;
    double compressionTimeMs;
    double decompressionTimeMs;
};

// A metric function writes one or more key-value pairs into the JSON object.
using MetricFn = std::function<void(const MetricContext&, nlohmann::json&)>;

// Global metric registry. Call registerMetric() to add a new metric.
std::vector<MetricFn>& getMetricRegistry();
void registerMetric(MetricFn fn);

struct BenchmarkResult {
    nlohmann::json metrics;
    std::vector<float> decompressedData;
};

// Run a chunked benchmark: compress/decompress data in chunkSizeBytes segments,
// then evaluate all registered metrics on the full original vs decompressed data.
// If chunkSizeBytes is 0 or >= total data size, processes as a single chunk.
// If iterations > 1, runs multiple times and averages timing results.
BenchmarkResult runChunkedBenchmark(
    Compressor& compressor,
    const std::vector<float>& data,
    std::size_t chunkSizeBytes,
    std::size_t iterations = 1,
    bool normalize = false);
