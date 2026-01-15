#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include "compressors/Compressor.hpp"
#include "compressors/factory.hpp"

// Test roundtrip with no truncation (should be lossless)
TEST(ZlibTruncCompressor, RoundTripNoTruncation) {
    auto compressor = createCompressor("zlib-trunc");
    compressor->configure({{"truncBits", "0"}});

    std::vector<float> original = {1.0f, 2.5f, 3.14159f, -42.0f, 0.0f};

    auto compressed = compressor->compress(original);
    auto decompressed = compressor->decompress(compressed);

    ASSERT_EQ(original.size(), decompressed.size());
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_FLOAT_EQ(original[i], decompressed[i]);
    }
}

// Test roundtrip with truncation (lossy)
TEST(ZlibTruncCompressor, RoundTripWithTruncation) {
    auto compressor = createCompressor("zlib-trunc");
    compressor->configure({{"truncBits", "8"}});

    std::vector<float> original = {1.0f, 2.5f, 3.14159f, -42.0f, 0.0f};

    auto compressed = compressor->compress(original);
    auto decompressed = compressor->decompress(compressed);

    ASSERT_EQ(original.size(), decompressed.size());
    for (size_t i = 0; i < original.size(); ++i) {
        // Values should be close but not exact (except 0 which truncates to 0)
        EXPECT_NEAR(original[i], decompressed[i], std::abs(original[i]) * 0.01f + 0.001f);
    }
}

// Test that empty input is handled gracefully
TEST(ZlibTruncCompressor, EmptyInput) {
    auto compressor = createCompressor("zlib-trunc");

    std::vector<float> empty;
    auto compressed = compressor->compress(empty);
    auto decompressed = compressor->decompress(compressed);

    EXPECT_TRUE(decompressed.empty());
}

// Test that factory creates the compressor correctly
TEST(Factory, CreatesZlibTrunc) {
    auto compressor = createCompressor("zlib-trunc");

    EXPECT_EQ(compressor->name(), "zlib-trunc");
}

// Test that configuration options are applied
TEST(ZlibTruncCompressor, Configure) {
    auto compressor = createCompressor("zlib-trunc");

    compressor->configure({{"compressionLevel", "9"}, {"truncBits", "12"}});
    auto config = compressor->getConfig();

    EXPECT_EQ(config["compressionLevel"], "9");
    EXPECT_EQ(config["truncBits"], "12");
}

// Test that unknown options throw
TEST(ZlibTruncCompressor, UnknownOptionThrows) {
    auto compressor = createCompressor("zlib-trunc");

    EXPECT_THROW(
        compressor->configure({{"unknownOption", "value"}}),
        std::invalid_argument
    );
}

// Test that invalid truncBits throws
TEST(ZlibTruncCompressor, InvalidTruncBitsThrows) {
    auto compressor = createCompressor("zlib-trunc");

    EXPECT_THROW(
        compressor->configure({{"truncBits", "24"}}),
        std::invalid_argument
    );

    EXPECT_THROW(
        compressor->configure({{"truncBits", "-1"}}),
        std::invalid_argument
    );
}