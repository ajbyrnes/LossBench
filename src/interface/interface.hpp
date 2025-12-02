#pragma once

#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "benchmark.hpp"

// Command-line configuration
struct Args {
    std::string dataFile;
    std::string treename;
    std::vector<std::string> branches;
    std::size_t chunkSize{0};

    std::string compressor;
    std::map<std::string, std::string> compressionOptions;

    std::string resultsFile;
    std::string decompFile;
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
    const BenchmarkResult& metrics,
    const CompressionResult& comp,
    std::string branch);

// Append a JSON object as a single line to a JSONL file.
void appendJSONL(const std::string& filepath, const nlohmann::json& entry);
