#include <algorithm>
#include <cmath>
#include <numeric>

#include "benchmark.hpp"

// Compute the Kolmogorov-Smirnov statistic between two samples.
// This is the maximum absolute difference between empirical CDFs.
static float computeKSStatistic(
    const std::vector<float>& original,
    const std::vector<float>& decompressed
) {
    std::size_t n = original.size();
    if (n == 0) return 0.0f;

    // Create sorted copies of both datasets
    std::vector<float> sortedOrig = original;
    std::vector<float> sortedDecomp = decompressed;
    std::sort(sortedOrig.begin(), sortedOrig.end());
    std::sort(sortedDecomp.begin(), sortedDecomp.end());

    // Merge both sorted arrays to get all unique points for CDF comparison
    std::vector<float> allPoints;
    allPoints.reserve(2 * n);
    allPoints.insert(allPoints.end(), sortedOrig.begin(), sortedOrig.end());
    allPoints.insert(allPoints.end(), sortedDecomp.begin(), sortedDecomp.end());
    std::sort(allPoints.begin(), allPoints.end());

    float maxDiff = 0.0f;
    std::size_t iOrig = 0, iDecomp = 0;

    for (float x : allPoints) {
        // Advance pointers to count values <= x
        while (iOrig < n && sortedOrig[iOrig] <= x) ++iOrig;
        while (iDecomp < n && sortedDecomp[iDecomp] <= x) ++iDecomp;

        // Empirical CDFs at point x
        float cdfOrig = static_cast<float>(iOrig) / n;
        float cdfDecomp = static_cast<float>(iDecomp) / n;

        maxDiff = std::max(maxDiff, std::abs(cdfOrig - cdfDecomp));
    }

    return maxDiff;
}

// Compute the Earth Mover's Distance (Wasserstein-1) between two 1D samples.
// For 1D distributions, this equals the integral of |CDF1 - CDF2|,
// which can be computed as the sum of |sorted1[i] - sorted2[i]| / n.
static float computeEarthMoverDistance(
    const std::vector<float>& original,
    const std::vector<float>& decompressed
) {
    std::size_t n = original.size();
    if (n == 0) return 0.0f;

    // Create sorted copies
    std::vector<float> sortedOrig = original;
    std::vector<float> sortedDecomp = decompressed;
    std::sort(sortedOrig.begin(), sortedOrig.end());
    std::sort(sortedDecomp.begin(), sortedDecomp.end());

    // EMD for equal-sized 1D samples is mean absolute difference of sorted values
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += std::abs(sortedOrig[i] - sortedDecomp[i]);
    }

    return static_cast<float>(sum / n);
}

// Compute Jensen-Shannon divergence between two samples using histograms.
// JSD = 0.5 * KL(P||M) + 0.5 * KL(Q||M), where M = 0.5 * (P + Q)
static float computeJensenShannonDivergence(
    const std::vector<float>& original,
    const std::vector<float>& decompressed,
    std::size_t numBins = 100
) {
    std::size_t n = original.size();
    if (n == 0) return 0.0f;

    // Find global min/max across both datasets
    float minVal = std::min(
        *std::min_element(original.begin(), original.end()),
        *std::min_element(decompressed.begin(), decompressed.end())
    );
    float maxVal = std::max(
        *std::max_element(original.begin(), original.end()),
        *std::max_element(decompressed.begin(), decompressed.end())
    );

    // Handle edge case where all values are the same
    if (maxVal == minVal) {
        return 0.0f;  // Identical distributions
    }

    float binWidth = (maxVal - minVal) / numBins;

    // Build histograms (counts)
    std::vector<double> histOrig(numBins, 0.0);
    std::vector<double> histDecomp(numBins, 0.0);

    auto getBin = [&](float val) -> std::size_t {
        std::size_t bin = static_cast<std::size_t>((val - minVal) / binWidth);
        return std::min(bin, numBins - 1);  // Clamp to last bin
    };

    for (std::size_t i = 0; i < n; ++i) {
        histOrig[getBin(original[i])] += 1.0;
        histDecomp[getBin(decompressed[i])] += 1.0;
    }

    // Convert to probability distributions (normalize)
    for (std::size_t i = 0; i < numBins; ++i) {
        histOrig[i] /= n;
        histDecomp[i] /= n;
    }

    // Compute JSD = 0.5 * KL(P||M) + 0.5 * KL(Q||M), where M = 0.5*(P+Q)
    double jsd = 0.0;
    for (std::size_t i = 0; i < numBins; ++i) {
        double p = histOrig[i];
        double q = histDecomp[i];
        double m = 0.5 * (p + q);

        if (m > 0.0) {
            if (p > 0.0) {
                jsd += 0.5 * p * std::log2(p / m);
            }
            if (q > 0.0) {
                jsd += 0.5 * q * std::log2(q / m);
            }
        }
    }

    return static_cast<float>(jsd);
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

    // Compute distribution-based metrics
    float ksStatistic = computeKSStatistic(original, decompResult.decompressedData);
    float earthMoverDistance = computeEarthMoverDistance(original, decompResult.decompressedData);
    float jensenShannonDivergence = computeJensenShannonDivergence(original, decompResult.decompressedData);

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
        .ksStatistic = ksStatistic,
        .earthMoverDistance = earthMoverDistance,
        .jensenShannonDivergence = jensenShannonDivergence,
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

    // Compute distribution-based metrics on the full data
    float ksStatistic = computeKSStatistic(data, allDecompressedData);
    float earthMoverDistance = computeEarthMoverDistance(data, allDecompressedData);
    float jensenShannonDivergence = computeJensenShannonDivergence(data, allDecompressedData);

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
        .ksStatistic = ksStatistic,
        .earthMoverDistance = earthMoverDistance,
        .jensenShannonDivergence = jensenShannonDivergence,
        .decompressedData = std::move(allDecompressedData)
    };
}
