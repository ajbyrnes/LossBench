#include <algorithm>
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

// Reconstruct nested vector<vector<float>> from flat data using entry sizes
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

        // Determine upfront whether any test writes decompressed output, so we
        // know to read entry sizes alongside the branch data.
        const bool anyDecompFile = std::ranges::any_of(
            args.tests, [](const TestConfig& t) { return !t.decompFile.empty(); });

        // Outer loop: one branch at a time so only one branch's data is in memory.
        // Future extension: TestConfig could carry a `branches` field specifying
        // which branches it needs, allowing multi-branch tests (e.g. invariant mass)
        // to load a group of branches together before running.
        for (const auto& branch : args.branches) {
            std::cout << "Reading data for branch '" << branch << "'...\n";
            std::vector<float> data = readVectorFloatBranchData(
                args.dataFile, args.treename, branch);

            std::vector<std::size_t> entrySizes;
            if (anyDecompFile) {
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

                    BenchmarkResult metrics = runChunkedBenchmark(
                        *compressor, data, test.chunkSize, test.iterations, test.normalize);

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

                        if (initializedDecompFiles.count(test.decompFile) == 0) {
                            createTreeWithVectorFloatBranch(
                                test.decompFile, args.treename, branch, branchData);
                            initializedDecompFiles.insert(test.decompFile);
                            std::cout << "  Created " << test.decompFile
                                      << " with branch '" << branch << "'\n";
                        } else {
                            insertVectorFloatBranch(
                                test.decompFile, args.treename, branch, branchData);
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