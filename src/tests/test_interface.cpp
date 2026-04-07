#include <gtest/gtest.h>

#include <cstring>
#include <stdexcept>
#include <vector>

#include "interface/interface.hpp"

// Helper to convert vector of strings to argc/argv format
class ArgvHelper {
public:
    explicit ArgvHelper(const std::vector<std::string>& args) {
        for (const auto& arg : args) {
            char* cstr = new char[arg.size() + 1];
            std::strcpy(cstr, arg.c_str());
            argv_.push_back(cstr);
        }
    }

    ~ArgvHelper() {
        for (char* ptr : argv_) {
            delete[] ptr;
        }
    }

    int argc() const { return static_cast<int>(argv_.size()); }
    char** argv() { return argv_.data(); }

private:
    std::vector<char*> argv_;
};

// Test parsing all required arguments
TEST(ParseArgs, AllRequired) {
    ArgvHelper args({"lossbench",
        "--inputFile", "data.root",
        "--tree", "Events",
        "--branches", "px",
        "--chunkSize", "1024",
        "--compressor", "zlib",
        "--resultsFile", "results.jsonl"});

    Args parsed = parseArgs(args.argc(), args.argv());

    EXPECT_EQ(parsed.dataFile, "data.root");
    EXPECT_EQ(parsed.treename, "Events");
    EXPECT_EQ(parsed.branches.size(), 1);
    EXPECT_EQ(parsed.branches[0], "px");
    EXPECT_EQ(parsed.chunkSize, 1024);
    EXPECT_EQ(parsed.compressor, "zlib");
    EXPECT_EQ(parsed.resultsFile, "results.jsonl");
}

// Test parsing compressor with options
TEST(ParseArgs, CompressorWithOptions) {
    ArgvHelper args({"lossbench",
        "--inputFile", "data.root",
        "--tree", "Events",
        "--branches", "px",
        "--chunkSize", "1024",
        "--compressor", "zlib:compressionLevel=9",
        "--resultsFile", "results.jsonl"});

    Args parsed = parseArgs(args.argc(), args.argv());

    EXPECT_EQ(parsed.compressor, "zlib");
    EXPECT_EQ(parsed.compressionOptions.size(), 1);
    EXPECT_EQ(parsed.compressionOptions.at("compressionLevel"), "9");
}

// Test parsing compressor with multiple options
TEST(ParseArgs, CompressorMultipleOptions) {
    ArgvHelper args({"lossbench",
        "--inputFile", "data.root",
        "--tree", "Events",
        "--branches", "px",
        "--chunkSize", "1024",
        "--compressor", "sz3:absErrorBound=0.01,errorBoundMode=1",
        "--resultsFile", "results.jsonl"});

    Args parsed = parseArgs(args.argc(), args.argv());

    EXPECT_EQ(parsed.compressor, "sz3");
    EXPECT_EQ(parsed.compressionOptions.size(), 2);
    EXPECT_EQ(parsed.compressionOptions.at("absErrorBound"), "0.01");
    EXPECT_EQ(parsed.compressionOptions.at("errorBoundMode"), "1");
}

// Test parsing multiple branches
TEST(ParseArgs, MultipleBranches) {
    ArgvHelper args({"lossbench",
        "--inputFile", "data.root",
        "--tree", "Events",
        "--branches", "px,py,pz",
        "--chunkSize", "1024",
        "--compressor", "zlib",
        "--resultsFile", "results.jsonl"});

    Args parsed = parseArgs(args.argc(), args.argv());

    ASSERT_EQ(parsed.branches.size(), 3);
    EXPECT_EQ(parsed.branches[0], "px");
    EXPECT_EQ(parsed.branches[1], "py");
    EXPECT_EQ(parsed.branches[2], "pz");
}

// Test that missing required argument throws
TEST(ParseArgs, MissingRequiredThrows) {
    // Missing --compressor
    ArgvHelper args({"lossbench",
        "--inputFile", "data.root",
        "--tree", "Events",
        "--branches", "px",
        "--chunkSize", "1024",
        "--resultsFile", "results.jsonl"});

    EXPECT_THROW(parseArgs(args.argc(), args.argv()), std::runtime_error);
}

// Test that unknown argument throws
TEST(ParseArgs, UnknownArgumentThrows) {
    ArgvHelper args({"lossbench",
        "--unknownArg", "value"});

    EXPECT_THROW(parseArgs(args.argc(), args.argv()), std::runtime_error);
}

// Test that malformed compressor option throws
TEST(ParseArgs, MalformedCompressorOptionThrows) {
    ArgvHelper args({"lossbench",
        "--inputFile", "data.root",
        "--tree", "Events",
        "--branches", "px",
        "--chunkSize", "1024",
        "--compressor", "zlib:badoption",  // missing =value
        "--resultsFile", "results.jsonl"});

    EXPECT_THROW(parseArgs(args.argc(), args.argv()), std::runtime_error);
}

// Test makeBenchmarkJSON has expected fields
TEST(MakeBenchmarkJSON, HasExpectedFields) {
    Args args;
    args.dataFile = "test.root";
    args.treename = "Events";
    args.branches = {"px"};
    args.resultsFile = "results.jsonl";

    TestConfig test;
    test.compressor = "zlib";
    test.chunkSize = 1024;

    std::map<std::string, std::string> compressorConfig = {{"compressionLevel", "6"}};

    BenchmarkResult metrics{};
    metrics.metrics["original_size_bytes"] = 400;  // 100 floats * 4 bytes
    metrics.metrics["compressed_size_bytes"] = 160;
    metrics.metrics["compression_ratio"] = 2.5f;
    metrics.metrics["compression_throughput_mbps"] = 100.0f;
    metrics.metrics["decompression_throughput_mbps"] = 200.0f;
    metrics.metrics["abs_error_max"] = 0.0f;
    metrics.metrics["abs_error_avg"] = 0.0f;

    nlohmann::json j = makeBenchmarkJSON(args, test, compressorConfig, metrics, "px");

    // Check structure exists
    EXPECT_TRUE(j.contains("system"));
    EXPECT_TRUE(j.contains("config"));
    EXPECT_TRUE(j.contains("results"));

    // Check config fields
    EXPECT_EQ(j["config"]["input_file"], "test.root");
    EXPECT_EQ(j["config"]["tree"], "Events");
    EXPECT_EQ(j["config"]["compressor"], "zlib");
    EXPECT_EQ(j["config"]["chunk_size"], 1024);

    // Check results fields
    EXPECT_EQ(j["results"]["compression_ratio"], 2.5f);
    EXPECT_EQ(j["results"]["original_size_bytes"], 400);  // 100 floats * 4 bytes
    EXPECT_EQ(j["results"]["compressed_size_bytes"], 160);
}
