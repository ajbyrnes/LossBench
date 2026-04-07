#include <algorithm>
#include <cmath>
#include <vector>

#include "benchmark.hpp"

// Kolmogorov-Smirnov test internals

struct KSTestResult {
    float statistic;
    float pValue;
};

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

static KSTestResult computeKSTest(
    const std::vector<float>& sortedOrig,
    const std::vector<float>& sortedDecomp
) {
    std::size_t n1 = sortedOrig.size();
    std::size_t n2 = sortedDecomp.size();
    if (n1 == 0 || n2 == 0) return {0.0f, 1.0f};

    std::vector<float> allPoints;
    allPoints.reserve(n1 + n2);
    allPoints.insert(allPoints.end(), sortedOrig.begin(), sortedOrig.end());
    allPoints.insert(allPoints.end(), sortedDecomp.begin(), sortedDecomp.end());
    std::sort(allPoints.begin(), allPoints.end());

    float maxDiff = 0.0f;
    std::size_t iOrig = 0, iDecomp = 0;

    for (float x : allPoints) {
        while (iOrig < n1 && sortedOrig[iOrig] <= x) ++iOrig;
        while (iDecomp < n2 && sortedDecomp[iDecomp] <= x) ++iDecomp;

        float cdfOrig = static_cast<float>(iOrig) / n1;
        float cdfDecomp = static_cast<float>(iDecomp) / n2;

        maxDiff = std::max(maxDiff, std::abs(cdfOrig - cdfDecomp));
    }

    double ne = static_cast<double>(n1) * n2 / (n1 + n2);
    double lambda = std::sqrt(ne) * maxDiff;
    float pValue = kolmogorovSurvival(lambda);

    return {maxDiff, pValue};
}

// Earth Mover's Distance (Wasserstein-1) for equal-sized sorted 1D samples
static float computeEarthMoverDistance(
    const std::vector<float>& sortedOrig,
    const std::vector<float>& sortedDecomp
) {
    std::size_t n = sortedOrig.size();
    if (n == 0) return 0.0f;

    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += std::abs(sortedOrig[i] - sortedDecomp[i]);
    }
    return static_cast<float>(sum / n);
}

// Quantile via linear interpolation on a sorted vector
static float quantile(const std::vector<float>& sorted, double p) {
    if (sorted.empty()) return 0.0f;
    double idx = p * (sorted.size() - 1);
    std::size_t lo = static_cast<std::size_t>(idx);
    std::size_t hi = std::min(lo + 1, sorted.size() - 1);
    double frac = idx - lo;
    return static_cast<float>(sorted[lo] * (1.0 - frac) + sorted[hi] * frac);
}

static auto reg = [] {
    registerMetric([](const MetricContext& ctx, nlohmann::json& out) {
        auto ks = computeKSTest(ctx.sortedOriginal, ctx.sortedDecompressed);
        out["ks_statistic"] = ks.statistic;
        out["ks_p_value"] = ks.pValue;

        out["wasserstein_distance"] =
            computeEarthMoverDistance(ctx.sortedOriginal, ctx.sortedDecompressed);

        out["q5_shift"]  = std::abs(quantile(ctx.sortedOriginal, 0.05) - quantile(ctx.sortedDecompressed, 0.05));
        out["q50_shift"] = std::abs(quantile(ctx.sortedOriginal, 0.50) - quantile(ctx.sortedDecompressed, 0.50));
        out["q95_shift"] = std::abs(quantile(ctx.sortedOriginal, 0.95) - quantile(ctx.sortedDecompressed, 0.95));
        out["q99_shift"] = std::abs(quantile(ctx.sortedOriginal, 0.99) - quantile(ctx.sortedDecompressed, 0.99));
    });
    return 0;
}();
