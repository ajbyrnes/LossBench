/// @file lossbench.cpp
/// @brief LossBench program entry point.
///
/// The driver flow is:
///   1. Handle info options (`--help`, `--list-compressors`, `--compressor-help`).
///   2. Parse the CLI into an @ref Args struct (one source file + tree + branches,
///      plus a list of @ref TestConfig entries to run).
///   3. For each branch, read its `vector<float>` data once and run every
///      configured test against it (compressor + options + chunk size + layout).
///   4. For each (branch, test) pair, append a JSON row to the results JSONL
///      and optionally write the decompressed data into a parallel ROOT file.
///
/// Per-branch iteration in the outer loop keeps at most one branch's data in
/// memory at a time; the inner loop reuses that data across every test.

#include <algorithm>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <TError.h>

#include "interface.hpp"
#include "root-utils.hpp"
#include "factory.hpp"
#include "benchmark.hpp"

/// Reconstruct the nested `vector<vector<float>>` shape (one inner vector per
/// TTree entry) from a flat decompressed buffer plus the original entry sizes.
std::vector<std::vector<float>> reconstructBranchData(
    const std::vector<float>& flatData,
    const std::vector<std::size_t>& entrySizes)
{
    std::vector<std::vector<float>> result;
    result.reserve(entrySizes.size());

    std::size_t offset = 0;
    for (std::size_t size : entrySizes) {
        result.emplace_back(flatData.begin() + offset, flatData.begin() + offset + size);
        offset += size;
    }

    return result;
}

/// Pack variable-length events into a zero-padded 2-D array (row-major).
///
/// Each event becomes a row of length @p maxLen, padded with zeros. Used to
/// give multi-dimensional backends (ZFP, SZ3, SPERR) a rectangular view of
/// inherently jagged HEP data.
std::vector<float> padTo2D(
    const std::vector<float>& flatData,
    const std::vector<std::size_t>& entrySizes,
    std::size_t maxLen)
{
    std::vector<float> padded(entrySizes.size() * maxLen, 0.0f);
    std::size_t srcOffset = 0;
    for (std::size_t i = 0; i < entrySizes.size(); ++i) {
        std::copy(flatData.begin() + srcOffset,
                  flatData.begin() + srcOffset + entrySizes[i],
                  padded.begin() + i * maxLen);
        srcOffset += entrySizes[i];
    }
    return padded;
}

/// Inverse of @ref padTo2D: strip the zero-padding from a 2-D buffer back to
/// the original variable-length per-event layout.
std::vector<float> unpadFrom2D(
    const std::vector<float>& paddedData,
    const std::vector<std::size_t>& entrySizes,
    std::size_t maxLen)
{
    std::vector<float> flat;
    std::size_t totalFloats = 0;
    for (auto s : entrySizes) totalFloats += s;
    flat.reserve(totalFloats);

    for (std::size_t i = 0; i < entrySizes.size(); ++i) {
        auto rowStart = paddedData.begin() + i * maxLen;
        flat.insert(flat.end(), rowStart, rowStart + entrySizes[i]);
    }
    return flat;
}

int main(int argc, char* argv[]) {
    gErrorIgnoreLevel = kFatal; // Suppress ROOT warnings for cleaner output; handle errors via exceptions instead.

    // Handle info options before parsing other args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage();
            std::cout << "\nInfo options:\n";
            std::cout << "  --list-compressors        List available compressors\n";
            std::cout << "  --compressor-help <name>  Show options for a compressor\n";
            std::cout << "  -h, --help                Show this help message\n";
            return 0;
        }

        if (arg == "--list-compressors") {
            std::cout << "Available compressors:\n";
            for (const auto& name : getAvailableCompressors()) {
                auto comp = createCompressor(name);
                std::cout << "  " << name << " - " << comp->description() << "\n";
            }
            return 0;
        }

        if (arg == "--compressor-help" && i + 1 < argc) {
            std::string name = argv[i + 1];
            try {
                auto comp = createCompressor(name);
                std::cout << comp->name() << " - " << comp->description() << "\n";
                std::cout << "Version: " << comp->version() << "\n\n";
                std::cout << comp->usage() << "\n";
            } catch (const std::invalid_argument&) {
                std::cerr << "Unknown compressor: " << name << "\n";
                std::cerr << "Use --list-compressors to see available compressors.\n";
                return 1;
            }
            return 0;
        }
    }

    try {
        Args args = parseArgs(argc, argv);
        //printArgs(args);

        // Track which decompFiles have been initialized: the first branch written
        // to a path creates the ROOT file; subsequent branches are inserted.
        // Each test should use a distinct decompFile path.
        std::set<std::string> initializedDecompFiles;

        // Determine upfront whether any test needs entry sizes: either for
        // writing decompressed output or for padded2d layout.
        const bool needEntrySizes = std::ranges::any_of(
            args.tests, [](const TestConfig& t) {
                return !t.decompFile.empty() || t.dataLayout == "padded2d";
            });

        // Outer loop: one branch at a time so only one branch's data is in memory.
        // Future extension: TestConfig could carry a `branches` field specifying
        // which branches it needs, allowing multi-branch tests (e.g. invariant mass)
        // to load a group of branches together before running.
        for (const auto& branch : args.branches) {
            std::cout << "Reading data for branch '" << branch << "'...\n";
            std::vector<float> data = readVectorFloatBranchData(
                args.dataFile, args.treename, branch);

            std::vector<std::size_t> entrySizes;
            if (needEntrySizes) {
                entrySizes = readBranchEntrySizes(args.dataFile, args.treename, branch);
            }

            // Inner loop: run every test against this branch's data.
            for (const auto& test : args.tests) {
                std::cout << "  Compressing branch '" << branch << "' with compressor '" << test.compressor << "'";
                if (!test.compressionOptions.empty()) {
                    std::cout << " (";
                    bool first = true;
                    for (const auto& [k, v] : test.compressionOptions) {
                        if (!first) std::cout << ", ";
                        std::cout << k << "=" << v;
                        first = false;
                    }
                    std::cout << ")";
                }
                std::cout << "...\n";

                try {
                    std::unique_ptr<Compressor> compressor = createCompressor(test.compressor);
                    compressor->configure(test.compressionOptions);

                    // Prepare data and dimensions based on layout
                    const std::vector<float>* benchData = &data;
                    std::vector<float> paddedData;
                    std::vector<std::size_t> dimensions;

                    if (test.dataLayout == "padded2d") {
                        std::size_t maxLen = *std::max_element(entrySizes.begin(), entrySizes.end());
                        std::size_t nRows = entrySizes.size();
                        paddedData = padTo2D(data, entrySizes, maxLen);
                        benchData = &paddedData;
                        dimensions = {nRows, maxLen};
                        std::cout << "    (padded2d: " << nRows << " x " << maxLen
                                  << ", padding overhead: "
                                  << std::format("{:.1f}%", 100.0 * (paddedData.size() - data.size()) / data.size())
                                  << ")\n";
                    }

                    BenchmarkResult metrics = runChunkedBenchmark(
                        *compressor, *benchData, test.chunkSize, test.iterations,
                        test.normalize, dimensions);

                    // If padded2d, unpad decompressed data back to original structure
                    if (test.dataLayout == "padded2d") {
                        std::size_t maxLen = dimensions[1];
                        metrics.decompressedData = unpadFrom2D(
                            metrics.decompressedData, entrySizes, maxLen);
                    }

                    std::map<std::string, std::string> compressorConfig = compressor->getConfig();
                    nlohmann::json resultJSON = makeBenchmarkJSON(
                        args, test, compressorConfig, metrics, branch);

                    if (!args.resultsFile.empty()) {
                        appendJSONL(args.resultsFile, resultJSON);
                        std::cout << "  Appended results to " << args.resultsFile << "\n";
                    }

                    if (!test.decompFile.empty()) {
                        std::vector<std::vector<float>> branchData =
                            reconstructBranchData(metrics.decompressedData, entrySizes);

                        if (initializedDecompFiles.count(test.decompFile) == 0
                            && !std::filesystem::exists(test.decompFile)) {
                            createTreeWithVectorFloatBranch(
                                test.decompFile, args.treename, branch, branchData);
                            initializedDecompFiles.insert(test.decompFile);
                            std::cout << "  Created " << test.decompFile
                                      << " with branch '" << branch << "'\n";
                        } else {
                            insertVectorFloatBranch(
                                test.decompFile, args.treename, branch, branchData);
                            initializedDecompFiles.insert(test.decompFile);
                            std::cout << "  Added branch '" << branch
                                      << "' to " << test.decompFile << "\n";
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "  Warning: test skipped - " << e.what() << "\n";
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Use --help for usage information.\n";
        return 1;
    }

    return 0;
}