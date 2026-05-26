/// @file benchmark.hpp
/// @brief Chunked benchmark driver plus the registry for benchmark metrics.
///
/// The benchmark layer is intentionally independent of any specific
/// compressor or metric: it only knows how to slice a `std::vector<float>`
/// into chunks, time each compress/decompress call, and then hand the
/// fully-reconstructed data to every metric registered via
/// @ref registerMetric.

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Compressor.hpp"

/// @brief Inputs available to every metric function.
///
/// Pre-sorted copies of the original and decompressed data are provided so
/// metrics that need order statistics (medians, quantiles, KS-style
/// distance) can share a single sort.
struct MetricContext {
    const std::vector<float>& original;            ///< Original (reference) floats.
    const std::vector<float>& decompressed;        ///< Decompressed floats.
    const std::vector<float>& sortedOriginal;      ///< Ascending sort of @ref original.
    const std::vector<float>& sortedDecompressed;  ///< Ascending sort of @ref decompressed.
    std::size_t originalSizeBytes;                 ///< Total bytes in the original input.
    std::size_t compressedSizeBytes;               ///< Total bytes emitted by the compressor.
    double compressionTimeMs;                      ///< Wall-clock compression time (ms).
    double decompressionTimeMs;                    ///< Wall-clock decompression time (ms).
};

/// @brief A metric function writes one or more key-value pairs into the
/// JSON object that will be appended to the results JSONL.
using MetricFn = std::function<void(const MetricContext&, nlohmann::json&)>;

/// @brief Process-wide registry of metric functions to evaluate per run.
std::vector<MetricFn>& getMetricRegistry();

/// @brief Append a new metric function to the global registry.
void registerMetric(MetricFn fn);

/// @brief Output of a single benchmark run: metrics plus reconstructed data.
struct BenchmarkResult {
    nlohmann::json metrics;                 ///< Aggregated metric results, ready to serialize.
    std::vector<float> decompressedData;    ///< Full reconstructed data (concatenated across chunks).
};

/// @brief Run a chunked compress/decompress benchmark and evaluate all
/// registered metrics on the full reconstructed data.
///
/// Compression and decompression are timed per chunk; the resulting times
/// are summed across chunks. If @p iterations > 1 the chunk loop is run
/// repeatedly and the times are averaged.
///
/// @param compressor      Compressor under test; its @ref Compressor::setDimensions
///                        is called once per chunk when @p dimensions is non-empty.
/// @param data            Flat input data.
/// @param chunkSizeBytes  Target chunk size in bytes. 0 or a value at least as
///                        large as the whole input means "process as a single chunk".
/// @param iterations      Number of timing repetitions; values < 1 are treated as 1.
/// @param normalize       If true, normalize the data before compression and undo
///                        the normalization after decompression.
/// @param dimensions      Optional logical shape; for 2-D inputs `{nRows, rowLen}`,
///                        chunks are row-aligned so multi-dimensional backends see
///                        valid sub-blocks.
BenchmarkResult runChunkedBenchmark(
    Compressor& compressor,
    const std::vector<float>& data,
    std::size_t chunkSizeBytes,
    std::size_t iterations = 1,
    bool normalize = false,
    const std::vector<std::size_t>& dimensions = {});
