#include <cmath>

#include "benchmark.hpp"

CompressionResult timedCompress(
    const Compressor& compressor,
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
    const Compressor& compressor,
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
    float compressionRatio = static_cast<float>(dataSizeBytes) / compResult.compressedData.bytes.size();
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
    float PSNR = (MSE > 0.0f) ? 10.0f * std::log10((1.0f * 1.0f) / MSE) : std::numeric_limits<float>::infinity();

    return {
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