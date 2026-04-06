#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "compressors/Compressor.hpp"
#include "compressors/factory.hpp"

// Test that compressing then decompressing returns the original data
TEST(ZstdCompressor, RoundTrip) {
    auto compressor = createCompressor("zstd");

    std::vector<float> original = {1.0f, 2.5f, 3.14159f, -42.0f, 0.0f};

    auto compressed = compressor->compress(original);
    auto decompressed = compressor->decompress(compressed);

    ASSERT_EQ(original.size(), decompressed.size());
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_FLOAT_EQ(original[i], decompressed[i]);
    }
}

// Test that empty input is handled gracefully
TEST(ZstdCompressor, EmptyInput) {
    auto compressor = createCompressor("zstd");

    std::vector<float> empty;
    auto compressed = compressor->compress(empty);
    auto decompressed = compressor->decompress(compressed);

    EXPECT_TRUE(decompressed.empty());
}

// Test that factory creates a compressor with the correct name
TEST(Factory, CreatesZstd) {
    auto compressor = createCompressor("zstd");

    EXPECT_EQ(compressor->name(), "zstd");
}

// Test that factory throws on unknown compressor name
TEST(Factory, InvalidNameThrows) {
    EXPECT_THROW(createCompressor("nonexistent"), std::invalid_argument);
}

// Test that getAvailableCompressors returns all compressors
TEST(Factory, GetAvailableCompressors) {
    auto compressors = getAvailableCompressors();

    // Should have at least 2 compressors (zstd, zstd-trunc always available)
    EXPECT_GE(compressors.size(), 2);

    // Should be sorted
    EXPECT_TRUE(std::is_sorted(compressors.begin(), compressors.end()));

    // Should contain core compressors
    EXPECT_NE(std::find(compressors.begin(), compressors.end(), "zstd"), compressors.end());
    EXPECT_NE(std::find(compressors.begin(), compressors.end(), "zstd-trunc"), compressors.end());

#ifdef HAS_SZ3
    // SZ3 should be available if compiled with SZ3 support
    EXPECT_NE(std::find(compressors.begin(), compressors.end(), "sz3"), compressors.end());
#endif
}

// Test that configuration options are applied
TEST(ZstdCompressor, Configure) {
    auto compressor = createCompressor("zstd");

    compressor->configure({{"compressionLevel", "9"}});
    auto config = compressor->getConfig();

    EXPECT_EQ(config["compressionLevel"], "9");
}

// Test that unknown options throw
TEST(ZstdCompressor, UnknownOptionThrows) {
    auto compressor = createCompressor("zstd");

    EXPECT_THROW(
        compressor->configure({{"unknownOption", "value"}}),
        std::invalid_argument
    );
}
