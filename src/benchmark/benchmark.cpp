#include <cmath>

#include "benchmark.hpp"

CompressionResult timedCompress(
    Compressor& compressor,
    const std::vector<float>& data
) 
{
    auto start = std::chrono::high_resolution_clock::now();
    CompressedData compressedData = compressor.compress(data);
    auto end = std::chrono::high_resolution_clock::now();
    return {
        .compressedData = std::move(compressedData),
        .elapsed = end - start
    };
}

DecompressionResult timedDecompress(
    Compressor& compressor,
    const CompressedData& compressedData
) 
{
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<float> decompressedData = compressor.decompress(compressedData);
    auto end = std::chrono::high_resolution_clock::now();
    return {
        .decompressedData = std::move(decompressedData),
        .elapsed = end - start
    };
}

BenchmarkResult computeBenchmarkMetrics(
    const std::vector<float>& original,
    const CompressionResult& compResult,
    const DecompressionResult& decompResult
)
{
    // Number of floats should be the same before and after
    if (original.size() != decompResult.decompressedData.size()) {
        throw std::runtime_error("Original and decompressed data size mismatch.");
    }

    size_t dataSizeBytes = original.size() * sizeof(float);
    float compressionRatio = static_cast<float>(dataSizeBytes) / compResult.compressedData.data.size();
    float compressionThroughputMbps = (dataSizeBytes / (1024.0f * 1024.0f)) / (compResult.elapsed.count() / 1000.0f);
    float decompressionThroughputMbps = (dataSizeBytes / (1024.0f * 1024.0f)) / (decompResult.elapsed.count() / 1000.0f);

    float absErrorMax = 0.0f;
    float absErrorSum = 0.0f;
    float relErrorMax = 0.0f;
    float relErrorSum = 0.0f;
    float mseSum = 0.0f;

    for (size_t i = 0; i < original.size(); ++i) {
        float absError = std::abs(original[i] - decompResult.decompressedData[i]);
        absErrorMax = std::max(absErrorMax, absError);
        absErrorSum += absError;

        float relError = (original[i] != 0.0f) ? absError / std::abs(original[i]) : 0.0f;
        relErrorMax = std::max(relErrorMax, relError);
        relErrorSum += relError;

        mseSum += absError * absError;
    }

    float absErrorAvg = absErrorSum / original.size();
    float relErrorAvg = relErrorSum / original.size();
    float MSE = mseSum / original.size();

    float maxVal = *std::max_element(original.begin(), original.end());
    float minVal = *std::min_element(original.begin(), original.end());
    float range = maxVal - minVal;

    float PSNR = (MSE > 0.0f) ? 10.0f * std::log10((range * range) / MSE) : std::numeric_limits<float>::infinity();

    return {
        .originalSizeBytes = dataSizeBytes,
        .compressedSizeBytes = compResult.compressedData.data.size(),
        .compressionRatio = compressionRatio,
        .compressionThroughputMbps = compressionThroughputMbps,
        .decompressionThroughputMbps = decompressionThroughputMbps,
        .absErrorMax = absErrorMax,
        .absErrorAvg = absErrorAvg,
        .relErrorMax = relErrorMax,
        .relErrorAvg = relErrorAvg,
        .MSE = MSE,
        .PSNR = PSNR
    };
}

BenchmarkResult runChunkedBenchmark(
    Compressor& compressor,
    const std::vector<float>& data,
    std::size_t chunkSizeBytes
)
{
    std::size_t totalFloats = data.size();
    std::size_t totalOriginalBytes = totalFloats * sizeof(float);

    // Calculate floats per chunk
    std::size_t floatsPerChunk = chunkSizeBytes / sizeof(float);

    // If chunkSizeBytes is 0 or >= total data size, process as single chunk
    if (floatsPerChunk == 0 || floatsPerChunk >= totalFloats) {
        floatsPerChunk = totalFloats;
    }

    // Compute global min/max for PSNR calculation upfront
    float globalMax = *std::max_element(data.begin(), data.end());
    float globalMin = *std::min_element(data.begin(), data.end());
    float globalRange = globalMax - globalMin;

    // Accumulators for aggregation
    std::size_t totalCompressedBytes = 0;
    std::chrono::duration<double, std::milli> totalCompressionTime{0};
    std::chrono::duration<double, std::milli> totalDecompressionTime{0};

    float absErrorMax = 0.0f;
    double absErrorSum = 0.0;
    float relErrorMax = 0.0f;
    double relErrorSum = 0.0;
    double mseSum = 0.0;

    // Process data in chunks
    for (std::size_t offset = 0; offset < totalFloats; offset += floatsPerChunk) {
        std::size_t chunkFloats = std::min(floatsPerChunk, totalFloats - offset);

        // Create chunk view
        std::vector<float> chunk(data.begin() + offset, data.begin() + offset + chunkFloats);

        // Compress chunk
        auto compStart = std::chrono::high_resolution_clock::now();
        CompressedData compressedChunk = compressor.compress(chunk);
        auto compEnd = std::chrono::high_resolution_clock::now();

        totalCompressedBytes += compressedChunk.data.size();
        totalCompressionTime += compEnd - compStart;

        // Decompress chunk
        auto decompStart = std::chrono::high_resolution_clock::now();
        std::vector<float> decompressedChunk = compressor.decompress(compressedChunk);
        auto decompEnd = std::chrono::high_resolution_clock::now();

        totalDecompressionTime += decompEnd - decompStart;

        // Compute error metrics for this chunk
        for (std::size_t i = 0; i < chunkFloats; ++i) {
            float absError = std::abs(chunk[i] - decompressedChunk[i]);
            absErrorMax = std::max(absErrorMax, absError);
            absErrorSum += absError;

            float relError = (chunk[i] != 0.0f) ? absError / std::abs(chunk[i]) : 0.0f;
            relErrorMax = std::max(relErrorMax, relError);
            relErrorSum += relError;

            mseSum += static_cast<double>(absError) * absError;
        }
    }

    // Compute final aggregated metrics
    float compressionRatio = static_cast<float>(totalOriginalBytes) / totalCompressedBytes;
    float compressionThroughputMbps = (totalOriginalBytes / (1024.0f * 1024.0f)) / (totalCompressionTime.count() / 1000.0f);
    float decompressionThroughputMbps = (totalOriginalBytes / (1024.0f * 1024.0f)) / (totalDecompressionTime.count() / 1000.0f);

    float absErrorAvg = static_cast<float>(absErrorSum / totalFloats);
    float relErrorAvg = static_cast<float>(relErrorSum / totalFloats);
    float MSE = static_cast<float>(mseSum / totalFloats);

    float PSNR = (MSE > 0.0f) ? 10.0f * std::log10((globalRange * globalRange) / MSE) : std::numeric_limits<float>::infinity();

    return {
        .originalSizeBytes = totalOriginalBytes,
        .compressedSizeBytes = totalCompressedBytes,
        .compressionRatio = compressionRatio,
        .compressionThroughputMbps = compressionThroughputMbps,
        .decompressionThroughputMbps = decompressionThroughputMbps,
        .absErrorMax = absErrorMax,
        .absErrorAvg = absErrorAvg,
        .relErrorMax = relErrorMax,
        .relErrorAvg = relErrorAvg,
        .MSE = MSE,
        .PSNR = PSNR
    };
}
