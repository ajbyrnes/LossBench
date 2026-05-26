/// @file error_metrics.cpp
/// @brief Point-wise error metrics (max/mean abs/rel, MSE, PSNR) registered at startup.

#include <algorithm>
#include <cmath>
#include <limits>

#include "benchmark.hpp"

static auto reg = [] {
    registerMetric([](const MetricContext& ctx, nlohmann::json& out) {
        std::size_t n = ctx.original.size();
        if (n == 0) return;

        float absMax = 0.0f, relMax = 0.0f;
        double absSum = 0.0, relSum = 0.0, mseSum = 0.0;

        for (std::size_t i = 0; i < n; ++i) {
            float ae = std::abs(ctx.original[i] - ctx.decompressed[i]);
            absMax = std::max(absMax, ae);
            absSum += ae;

            float re = (ctx.original[i] != 0.0f)
                ? ae / std::abs(ctx.original[i]) : 0.0f;
            relMax = std::max(relMax, re);
            relSum += re;

            mseSum += static_cast<double>(ae) * ae;
        }

        out["abs_error_max"] = absMax;
        out["abs_error_avg"] = static_cast<float>(absSum / n);
        out["rel_error_max"] = relMax;
        out["rel_error_avg"] = static_cast<float>(relSum / n);

        float mse = static_cast<float>(mseSum / n);
        out["mse"] = mse;

        float globalMax = *std::max_element(ctx.original.begin(), ctx.original.end());
        float globalMin = *std::min_element(ctx.original.begin(), ctx.original.end());
        float range = globalMax - globalMin;

        out["psnr"] = (mse > 0.0f)
            ? 10.0f * std::log10(range * range / mse)
            : std::numeric_limits<float>::infinity();
    });
    return 0;
}();
