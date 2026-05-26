/// @file benchmark.cpp
/// @brief Implementation of the chunked benchmark loop and metric registry.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <vector>

#include "benchmark.hpp"

std::vector<MetricFn>& getMetricRegistry() {
    static std::vector<MetricFn> registry;
    return registry;
}

void registerMetric(MetricFn fn) {
    getMetricRegistry().push_back(std::move(fn));
}

BenchmarkResult runChunkedBenchmark(
    Compressor& compressor,
    const std::vector<float>& data,
    std::size_t chunkSizeBytes,
    std::size_t iterations,
    bool normalize,
    const std::vector<std::size_t>& dimensions
)
{
    if (iterations == 0) {
        iterations = 1;
    }

    std::size_t totalFloats = data.size();
    std::size_t totalOriginalBytes = totalFloats * sizeof(float);

    // Determine if data is 2D (dimensions = {nRows, rowLen})
    const bool is2D = (dimensions.size() == 2 && dimensions[0] > 0 && dimensions[1] > 0);
    const std::size_t rowLen = is2D ? dimensions[1] : 0;

    // Calculate floats per chunk
    std::size_t floatsPerChunk = chunkSizeBytes / sizeof(float);

    // If chunkSizeBytes is 0 or >= total data size, process as single chunk
    if (floatsPerChunk == 0 || floatsPerChunk >= totalFloats) {
        floatsPerChunk = totalFloats;
    }

    // For 2D data, round floatsPerChunk down to a multiple of rowLen
    // so each chunk contains complete rows
    if (is2D && floatsPerChunk < totalFloats && rowLen > 0) {
        floatsPerChunk = (floatsPerChunk / rowLen) * rowLen;
        if (floatsPerChunk == 0) {
            floatsPerChunk = rowLen; // at least one row per chunk
        }
    }

    // Accumulators across all iterations for timing
    std::chrono::duration<double, std::milli> totalCompressionTime{0};
    std::chrono::duration<double, std::milli> totalDecompressionTime{0};

    std::size_t totalCompressedBytes = 0;
    std::vector<float> allDecompressedData;

    for (std::size_t iter = 0; iter < iterations; ++iter) {
        std::size_t iterCompressedBytes = 0;
        std::chrono::duration<double, std::milli> iterCompressionTime{0};
        std::chrono::duration<double, std::milli> iterDecompressionTime{0};

        // Only collect decompressed data on the last iteration (deterministic)
        bool lastIter = (iter == iterations - 1);
        if (lastIter) {
            allDecompressedData.clear();
            allDecompressedData.reserve(totalFloats);
        }

        // Process data in chunks
        for (std::size_t offset = 0; offset < totalFloats; offset += floatsPerChunk) {
            std::size_t chunkFloats = std::min(floatsPerChunk, totalFloats - offset);

            std::vector<float> chunk(data.begin() + offset, data.begin() + offset + chunkFloats);

            // Normalize chunk to [0, 1] if requested
            float normMin = 0.0f, normRange = 1.0f;
            if (normalize) {
                auto [minIt, maxIt] = std::minmax_element(chunk.begin(), chunk.end());
                normMin = *minIt;
                float normMax = *maxIt;
                normRange = normMax - normMin;
                if (normRange > 0.0f) {
                    for (auto& v : chunk) {
                        v = (v - normMin) / normRange;
                    }
                }
            }

            // Set dimensions for this chunk
            if (is2D) {
                std::size_t chunkRows = chunkFloats / rowLen;
                compressor.setDimensions({chunkRows, rowLen});
            } else if (!dimensions.empty()) {
                compressor.setDimensions({chunkFloats});
            }

            // Compress
            auto compStart = std::chrono::steady_clock::now();
            CompressedData compressedChunk = compressor.compress(chunk);
            auto compEnd = std::chrono::steady_clock::now();

            iterCompressedBytes += compressedChunk.data.size();
            iterCompressionTime += compEnd - compStart;

            // Set dimensions again before decompress (in case compressor state changed)
            if (is2D) {
                std::size_t chunkRows = chunkFloats / rowLen;
                compressor.setDimensions({chunkRows, rowLen});
            } else if (!dimensions.empty()) {
                compressor.setDimensions({chunkFloats});
            }

            // Decompress
            auto decompStart = std::chrono::steady_clock::now();
            std::vector<float> decompressedChunk = compressor.decompress(compressedChunk);
            auto decompEnd = std::chrono::steady_clock::now();

            iterDecompressionTime += decompEnd - decompStart;

            // Denormalize back to original range
            if (normalize && normRange > 0.0f) {
                for (auto& v : decompressedChunk) {
                    v = v * normRange + normMin;
                }
            }

            if (lastIter) {
                allDecompressedData.insert(allDecompressedData.end(),
                    decompressedChunk.begin(), decompressedChunk.end());
            }
        }

        totalCompressionTime += iterCompressionTime;
        totalDecompressionTime += iterDecompressionTime;
        totalCompressedBytes = iterCompressedBytes;  // Same each iteration
    }

    // Average timing across iterations
    totalCompressionTime /= iterations;
    totalDecompressionTime /= iterations;

    // Pre-sort for distribution-based metrics
    std::vector<float> sortedOrig = data;
    std::vector<float> sortedDecomp = allDecompressedData;
    std::sort(sortedOrig.begin(), sortedOrig.end());
    std::sort(sortedDecomp.begin(), sortedDecomp.end());

    MetricContext ctx{
        .original = data,
        .decompressed = allDecompressedData,
        .sortedOriginal = sortedOrig,
        .sortedDecompressed = sortedDecomp,
        .originalSizeBytes = totalOriginalBytes,
        .compressedSizeBytes = totalCompressedBytes,
        .compressionTimeMs = totalCompressionTime.count(),
        .decompressionTimeMs = totalDecompressionTime.count()
    };

    nlohmann::json metrics;
    for (auto& fn : getMetricRegistry()) {
        fn(ctx, metrics);
    }

    return {std::move(metrics), std::move(allDecompressedData)};
}
