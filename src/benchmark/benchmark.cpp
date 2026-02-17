#include <algorithm>
#include <cmath>
#include <numeric>

#include "benchmark.hpp"

// Result of a two-sample Kolmogorov-Smirnov test.
struct KSTestResult {
    float statistic;  // Max absolute difference between empirical CDFs
    float pValue;     // Asymptotic p-value
};

// Compute the asymptotic p-value for the Kolmogorov-Smirnov test
// using the survival function of the Kolmogorov distribution.
// P(D > d) = 2 * sum_{k=1}^{inf} (-1)^{k-1} * exp(-2 * k^2 * lambda^2)
static float kolmogorovSurvival(double lambda) {
    if (lambda <= 0.0) return 1.0f;

    double sum = 0.0;
    for (int k = 1; k <= 100; ++k) {
        double term = std::exp(-2.0 * k * k * lambda * lambda);
        if (k % 2 == 1) {
            sum += term;
        } else {
            sum -= term;
        }
        if (term < 1e-12) break;
    }

    double pValue = 2.0 * sum;
    return static_cast<float>(std::clamp(pValue, 0.0, 1.0));
}

// Perform a two-sample Kolmogorov-Smirnov test between original and decompressed data.
// Returns both the KS statistic and the asymptotic p-value.
static KSTestResult computeKSTest(
    const std::vector<float>& original,
    const std::vector<float>& decompressed
) {
    std::size_t n1 = original.size();
    std::size_t n2 = decompressed.size();
    if (n1 == 0 || n2 == 0) return {0.0f, 1.0f};

    // Create sorted copies of both datasets
    std::vector<float> sortedOrig = original;
    std::vector<float> sortedDecomp = decompressed;
    std::sort(sortedOrig.begin(), sortedOrig.end());
    std::sort(sortedDecomp.begin(), sortedDecomp.end());

    // Merge both sorted arrays to get all points for CDF comparison
    std::vector<float> allPoints;
    allPoints.reserve(n1 + n2);
    allPoints.insert(allPoints.end(), sortedOrig.begin(), sortedOrig.end());
    allPoints.insert(allPoints.end(), sortedDecomp.begin(), sortedDecomp.end());
    std::sort(allPoints.begin(), allPoints.end());

    float maxDiff = 0.0f;
    std::size_t iOrig = 0, iDecomp = 0;

    for (float x : allPoints) {
        // Advance pointers to count values <= x
        while (iOrig < n1 && sortedOrig[iOrig] <= x) ++iOrig;
        while (iDecomp < n2 && sortedDecomp[iDecomp] <= x) ++iDecomp;

        // Empirical CDFs at point x
        float cdfOrig = static_cast<float>(iOrig) / n1;
        float cdfDecomp = static_cast<float>(iDecomp) / n2;

        maxDiff = std::max(maxDiff, std::abs(cdfOrig - cdfDecomp));
    }

    // Compute asymptotic p-value
    double ne = static_cast<double>(n1) * n2 / (n1 + n2);
    double lambda = (std::sqrt(ne) + 0.12 + 0.11 / std::sqrt(ne)) * maxDiff;
    float pValue = kolmogorovSurvival(lambda);

    return {maxDiff, pValue};
}

CompressionResult timedCompress(
    Compressor& compressor,
    const std::vector<float>& data
) 
{
    auto start = std::chrono::steady_clock::now();
    CompressedData compressedData = compressor.compress(data);
    auto end = std::chrono::steady_clock::now();
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
    auto start = std::chrono::steady_clock::now();
    std::vector<float> decompressedData = compressor.decompress(compressedData);
    auto end = std::chrono::steady_clock::now();
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

    // Compute distribution-based metrics (two-sample KS test)
    auto ksResult = computeKSTest(original, decompResult.decompressedData);

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
        .PSNR = PSNR,
        .ksStatistic = ksResult.statistic,
        .ksPValue = ksResult.pValue,
        .decompressedData = decompResult.decompressedData
    };
}

BenchmarkResult runChunkedBenchmark(
    Compressor& compressor,
    const std::vector<float>& data,
    std::size_t chunkSizeBytes,
    std::size_t iterations
)
{
    if (iterations == 0) {
        iterations = 1;
    }

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

    // Accumulators across all iterations for timing
    std::chrono::duration<double, std::milli> totalCompressionTime{0};
    std::chrono::duration<double, std::milli> totalDecompressionTime{0};

    // These are computed from the last iteration (deterministic across iterations)
    std::size_t totalCompressedBytes = 0;
    float absErrorMax = 0.0f;
    double absErrorSum = 0.0;
    float relErrorMax = 0.0f;
    double relErrorSum = 0.0;
    double mseSum = 0.0;
    std::vector<float> allDecompressedData;

    for (std::size_t iter = 0; iter < iterations; ++iter) {
        // Reset per-iteration accumulators
        std::size_t iterCompressedBytes = 0;
        std::chrono::duration<double, std::milli> iterCompressionTime{0};
        std::chrono::duration<double, std::milli> iterDecompressionTime{0};

        // Only compute error metrics on the last iteration (they're deterministic)
        bool computeErrors = (iter == iterations - 1);
        if (computeErrors) {
            absErrorMax = 0.0f;
            absErrorSum = 0.0;
            relErrorMax = 0.0f;
            relErrorSum = 0.0;
            mseSum = 0.0;
            allDecompressedData.clear();
            allDecompressedData.reserve(totalFloats);
        }

        // Process data in chunks
        for (std::size_t offset = 0; offset < totalFloats; offset += floatsPerChunk) {
            std::size_t chunkFloats = std::min(floatsPerChunk, totalFloats - offset);

            // Create chunk view
            std::vector<float> chunk(data.begin() + offset, data.begin() + offset + chunkFloats);

            // Compress chunk
            auto compStart = std::chrono::steady_clock::now();
            CompressedData compressedChunk = compressor.compress(chunk);
            auto compEnd = std::chrono::steady_clock::now();

            iterCompressedBytes += compressedChunk.data.size();
            iterCompressionTime += compEnd - compStart;

            // Decompress chunk
            auto decompStart = std::chrono::steady_clock::now();
            std::vector<float> decompressedChunk = compressor.decompress(compressedChunk);
            auto decompEnd = std::chrono::steady_clock::now();

            iterDecompressionTime += decompEnd - decompStart;

            if (computeErrors) {
                // Accumulate decompressed data
                allDecompressedData.insert(allDecompressedData.end(),
                    decompressedChunk.begin(), decompressedChunk.end());

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
        }

        // Accumulate timing across iterations
        totalCompressionTime += iterCompressionTime;
        totalDecompressionTime += iterDecompressionTime;
        totalCompressedBytes = iterCompressedBytes;  // Same each iteration
    }

    // Average timing across iterations
    totalCompressionTime /= iterations;
    totalDecompressionTime /= iterations;

    // Compute final aggregated metrics
    float compressionRatio = static_cast<float>(totalOriginalBytes) / totalCompressedBytes;
    float compressionThroughputMbps = (totalOriginalBytes / (1024.0f * 1024.0f)) / (totalCompressionTime.count() / 1000.0f);
    float decompressionThroughputMbps = (totalOriginalBytes / (1024.0f * 1024.0f)) / (totalDecompressionTime.count() / 1000.0f);

    float absErrorAvg = static_cast<float>(absErrorSum / totalFloats);
    float relErrorAvg = static_cast<float>(relErrorSum / totalFloats);
    float MSE = static_cast<float>(mseSum / totalFloats);

    float PSNR = (MSE > 0.0f) ? 10.0f * std::log10((globalRange * globalRange) / MSE) : std::numeric_limits<float>::infinity();

    // Compute distribution-based metrics on the full data (two-sample KS test)
    auto ksResult = computeKSTest(data, allDecompressedData);

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
        .PSNR = PSNR,
        .ksStatistic = ksResult.statistic,
        .ksPValue = ksResult.pValue,
        .decompressedData = std::move(allDecompressedData)
    };
}
