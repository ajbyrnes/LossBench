#pragma once

#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "benchmark.hpp"

// Configuration for a single compression test.
struct TestConfig {
    std::string compressor;
    std::map<std::string, std::string> compressionOptions;
    std::size_t chunkSize{0};
    std::size_t iterations{1};
    std::string decompFile;
    bool normalize{true};
};

// Global configuration: data source and list of tests to run.
// Tests are run per-branch so only one branch's data lives in memory at a time.
// Future extension: add a `branches` field to TestConfig to specify which
// branches a test operates on, enabling multi-branch tests (e.g. invariant mass).
struct Args {
    std::string dataFile;
    std::string treename;
    std::vector<std::string> branches;
    std::string resultsFile;
    std::vector<TestConfig> tests;
};

// Parse command-line arguments into Args; throws std::runtime_error on error.
Args parseArgs(int argc, char* argv[]);

// Print usage/help to stdout.
void printUsage();

// Print parsed arguments for debugging.
void printArgs(const Args& args);

// Build a JSON object representing benchmark outputs.
nlohmann::json makeBenchmarkJSON(
    const Args& args,
    const TestConfig& test,
    const std::map<std::string, std::string>& compressorConfig,
    const BenchmarkResult& metrics,
    std::string branch);

// Append a JSON object as a single line to a JSONL file.
void appendJSONL(const std::string& filepath, const nlohmann::json& entry);