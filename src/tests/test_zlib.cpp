#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "compressors/Compressor.hpp"
#include "compressors/factory.hpp"

// Test that compressing then decompressing returns the original data
TEST(ZlibCompressor, RoundTrip) {
    auto compressor = createCompressor("zlib");

    std::vector<float> original = {1.0f, 2.5f, 3.14159f, -42.0f, 0.0f};

    auto compressed = compressor->compress(original);
    auto decompressed = compressor->decompress(compressed);

    ASSERT_EQ(original.size(), decompressed.size());
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_FLOAT_EQ(original[i], decompressed[i]);
    }
}

// Test that empty input is handled gracefully
TEST(ZlibCompressor, EmptyInput) {
    auto compressor = createCompressor("zlib");

    std::vector<float> empty;
    auto compressed = compressor->compress(empty);
    auto decompressed = compressor->decompress(compressed);

    EXPECT_TRUE(decompressed.empty());
}

// Test that factory creates a compressor with the correct name
TEST(Factory, CreatesZlib) {
    auto compressor = createCompressor("zlib");

    EXPECT_EQ(compressor->name(), "zlib");
}

// Test that factory throws on unknown compressor name
TEST(Factory, InvalidNameThrows) {
    EXPECT_THROW(createCompressor("nonexistent"), std::invalid_argument);
}

// Test that configuration options are applied
TEST(ZlibCompressor, Configure) {
    auto compressor = createCompressor("zlib");

    compressor->configure({{"compressionLevel", "9"}});
    auto config = compressor->getConfig();

    EXPECT_EQ(config["compressionLevel"], "9");
}