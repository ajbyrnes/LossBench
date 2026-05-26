/// @file throughput_metrics.cpp
/// @brief Compression ratio and compression/decompression throughput metrics.

#include "benchmark.hpp"

static auto reg = [] {
    registerMetric([](const MetricContext& ctx, nlohmann::json& out) {
        float sizeMB = ctx.originalSizeBytes / (1024.0f * 1024.0f);

        out["original_size_bytes"] = ctx.originalSizeBytes;
        out["compressed_size_bytes"] = ctx.compressedSizeBytes;
        out["compression_ratio"] =
            static_cast<float>(ctx.originalSizeBytes) / ctx.compressedSizeBytes;
        out["compression_throughput_mbps"] =
            sizeMB / (static_cast<float>(ctx.compressionTimeMs) / 1000.0f);
        out["decompression_throughput_mbps"] =
            sizeMB / (static_cast<float>(ctx.decompressionTimeMs) / 1000.0f);
    });
    return 0;
}();
