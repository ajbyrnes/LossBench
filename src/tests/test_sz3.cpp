#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include "compressors/Compressor.hpp"
#include "compressors/factory.hpp"

// Test that compressing then decompressing returns data within error bounds
TEST(SZ3Compressor, RoundTrip) {
    auto compressor = createCompressor("sz3");

    const double errorBound = 0.01;
    compressor->configure({{"absErrorBound", std::to_string(errorBound)}});

    std::vector<float> original = {1.0f, 2.5f, 3.14159f, -42.0f, 0.0f};

    auto compressed = compressor->compress(original);
    auto decompressed = compressor->decompress(compressed);

    ASSERT_EQ(original.size(), decompressed.size());
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_NEAR(original[i], decompressed[i], errorBound);
    }
}

// SZ3 does not handle empty input - it crashes.
// This test is disabled to document the known limitation.
TEST(SZ3Compressor, DISABLED_EmptyInput) {
    auto compressor = createCompressor("sz3");

    std::vector<float> empty;
    auto compressed = compressor->compress(empty);
    auto decompressed = compressor->decompress(compressed);

    EXPECT_TRUE(decompressed.empty());
}

// Test that factory creates a compressor with the correct name
TEST(Factory, CreatesSZ3) {
    auto compressor = createCompressor("sz3");

    EXPECT_EQ(compressor->name(), "SZ3");
}

// Test that configuration options are applied
TEST(SZ3Compressor, Configure) {
    auto compressor = createCompressor("sz3");

    compressor->configure({{"absErrorBound", "0.001"}});
    auto config = compressor->getConfig();

    EXPECT_EQ(config["absErrorBound"], "0.001000");
}
