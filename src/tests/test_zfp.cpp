#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include "compressors/Compressor.hpp"
#include "compressors/factory.hpp"

// Test that factory creates a compressor with the correct name
TEST(ZFPCompressor, FactoryCreatesZFP) {
    auto compressor = createCompressor("zfp");
    EXPECT_EQ(compressor->name(), "ZFP");
}

// Test round trip with accuracy mode (default)
TEST(ZFPCompressor, RoundTripAccuracy) {
    auto compressor = createCompressor("zfp");

    const double tolerance = 0.01;
    compressor->configure({{"mode", "accuracy"}, {"tolerance", std::to_string(tolerance)}});

    std::vector<float> original = {1.0f, 2.5f, 3.14159f, -42.0f, 0.0f, 100.5f, -0.001f};

    auto compressed = compressor->compress(original);
    auto decompressed = compressor->decompress(compressed);

    ASSERT_EQ(original.size(), decompressed.size());
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_NEAR(original[i], decompressed[i], tolerance);
    }
}

// Test round trip with rate mode
TEST(ZFPCompressor, RoundTripRate) {
    auto compressor = createCompressor("zfp");

    compressor->configure({{"mode", "rate"}, {"rate", "16"}});

    std::vector<float> original = {1.0f, 2.5f, 3.14159f, -42.0f, 0.0f, 100.5f, -0.001f};

    auto compressed = compressor->compress(original);
    auto decompressed = compressor->decompress(compressed);

    ASSERT_EQ(original.size(), decompressed.size());
    // Rate mode doesn't guarantee exact error bounds, just check values are reasonable
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_NEAR(original[i], decompressed[i], std::abs(original[i]) * 0.1 + 0.1);
    }
}

// Test round trip with precision mode
TEST(ZFPCompressor, RoundTripPrecision) {
    auto compressor = createCompressor("zfp");

    compressor->configure({{"mode", "precision"}, {"precision", "20"}});

    std::vector<float> original = {1.0f, 2.5f, 3.14159f, -42.0f, 0.0f, 100.5f, -0.001f};

    auto compressed = compressor->compress(original);
    auto decompressed = compressor->decompress(compressed);

    ASSERT_EQ(original.size(), decompressed.size());
    // Precision mode with 20 bits should be fairly accurate
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_NEAR(original[i], decompressed[i], std::abs(original[i]) * 0.001 + 0.001);
    }
}

// Test round trip with reversible (lossless) mode
TEST(ZFPCompressor, RoundTripReversible) {
    auto compressor = createCompressor("zfp");

    compressor->configure({{"mode", "reversible"}});

    std::vector<float> original = {1.0f, 2.5f, 3.14159f, -42.0f, 0.0f, 100.5f, -0.001f};

    auto compressed = compressor->compress(original);
    auto decompressed = compressor->decompress(compressed);

    ASSERT_EQ(original.size(), decompressed.size());
    // Reversible mode should be lossless
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_EQ(original[i], decompressed[i]);
    }
}

// Test that configuration options are applied correctly
TEST(ZFPCompressor, ConfigureMode) {
    auto compressor = createCompressor("zfp");

    compressor->configure({{"mode", "rate"}, {"rate", "8.5"}});
    auto config = compressor->getConfig();

    EXPECT_EQ(config["mode"], "rate");
    EXPECT_EQ(config["rate"], "8.500000");
}

TEST(ZFPCompressor, ConfigurePrecision) {
    auto compressor = createCompressor("zfp");

    compressor->configure({{"mode", "precision"}, {"precision", "24"}});
    auto config = compressor->getConfig();

    EXPECT_EQ(config["mode"], "precision");
    EXPECT_EQ(config["precision"], "24");
}

TEST(ZFPCompressor, ConfigureAccuracy) {
    auto compressor = createCompressor("zfp");

    compressor->configure({{"mode", "accuracy"}, {"tolerance", "0.001"}});
    auto config = compressor->getConfig();

    EXPECT_EQ(config["mode"], "accuracy");
    EXPECT_EQ(config["tolerance"], "0.001000");
}

TEST(ZFPCompressor, ConfigureReversible) {
    auto compressor = createCompressor("zfp");

    compressor->configure({{"mode", "reversible"}});
    auto config = compressor->getConfig();

    EXPECT_EQ(config["mode"], "reversible");
}

// Test that unknown options throw
TEST(ZFPCompressor, UnknownOptionThrows) {
    auto compressor = createCompressor("zfp");

    EXPECT_THROW(
        compressor->configure({{"unknownOption", "value"}}),
        std::invalid_argument
    );
}

// Test that invalid mode throws
TEST(ZFPCompressor, InvalidModeThrows) {
    auto compressor = createCompressor("zfp");

    EXPECT_THROW(
        compressor->configure({{"mode", "invalid"}}),
        std::invalid_argument
    );
}

// Test that invalid rate throws
TEST(ZFPCompressor, InvalidRateThrows) {
    auto compressor = createCompressor("zfp");

    EXPECT_THROW(
        compressor->configure({{"rate", "-1"}}),
        std::invalid_argument
    );
}

// Test that invalid precision throws
TEST(ZFPCompressor, InvalidPrecisionThrows) {
    auto compressor = createCompressor("zfp");

    EXPECT_THROW(
        compressor->configure({{"precision", "0"}}),
        std::invalid_argument
    );
}

// Test that invalid tolerance throws
TEST(ZFPCompressor, InvalidToleranceThrows) {
    auto compressor = createCompressor("zfp");

    EXPECT_THROW(
        compressor->configure({{"tolerance", "-0.001"}}),
        std::invalid_argument
    );
}