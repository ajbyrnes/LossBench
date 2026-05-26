/// @file interface.hpp
/// @brief CLI argument parsing and JSONL result serialization.
///
/// The interface layer is the only place that knows about the command-line
/// syntax (`--inputFile`, `--branches`, `--compressor`, ...) and about the
/// JSONL output format. Everything downstream operates on the populated
/// @ref Args / @ref TestConfig structs.

#pragma once

#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "benchmark.hpp"

/// @brief Configuration for a single compression test.
///
/// One @ref TestConfig describes one (compressor, options, layout) tuple
/// that the driver will apply to every selected branch. Multiple tests can
/// be queued in a single program invocation; each one produces its own
/// JSONL row per branch.
struct TestConfig {
    std::string compressor;                                  ///< CLI name of the compressor to use.
    std::map<std::string, std::string> compressionOptions;   ///< Raw `key=value` options forwarded to the compressor.
    std::size_t chunkSize{0};                                ///< Chunk size in bytes; 0 means "one chunk".
    std::size_t iterations{1};                               ///< Timing repetitions for this test.
    std::string decompFile;                                  ///< Optional ROOT file path to write decompressed data into.
    bool normalize{false};                                   ///< If true, normalize before compression and unnormalize after.
    std::string dataLayout = "flat";                         ///< Layout: "flat" or "padded2d".
};

/// @brief Global configuration: data source plus list of tests to run.
///
/// Tests are run per-branch so only one branch's data lives in memory at a
/// time. Future extension: add a `branches` field to @ref TestConfig to
/// specify which branches a test operates on, enabling multi-branch tests
/// (e.g. invariant mass).
struct Args {
    std::string dataFile;                  ///< Path to the input `.root` file.
    std::string treename;                  ///< TTree name inside @ref dataFile.
    std::vector<std::string> branches;     ///< Branches to read and benchmark.
    std::string resultsFile;               ///< Output `.jsonl` path; results are appended.
    std::vector<TestConfig> tests;         ///< Tests to run against each branch.
};

/// @brief Parse command-line arguments into @ref Args.
/// @throws std::runtime_error on missing/invalid arguments.
Args parseArgs(int argc, char* argv[]);

/// @brief Print top-level CLI usage to stdout.
void printUsage();

/// @brief Print parsed arguments in human-readable form (debugging aid).
void printArgs(const Args& args);

/// @brief Build a JSON object capturing the inputs, configuration, and
/// metrics for a single benchmark run.
///
/// The shape of this object is the JSONL row appended to the results file.
nlohmann::json makeBenchmarkJSON(
    const Args& args,
    const TestConfig& test,
    const std::map<std::string, std::string>& compressorConfig,
    const BenchmarkResult& metrics,
    std::string branch);

/// @brief Append a JSON object as a single line to the JSONL file at @p filepath.
///
/// Creates the file if it does not exist; otherwise appends without
/// touching previous lines.
void appendJSONL(const std::string& filepath, const nlohmann::json& entry);
