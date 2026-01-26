#include <format>
#include <iostream>
#include <memory>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

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
        // Parse command line arguments
        Args args = parseArgs(argc, argv);
        printArgs(args);

        // Create compressor
        std::unique_ptr<Compressor> compressor = createCompressor(args.compressor);
        compressor->configure(args.compressionOptions);

        // Track whether we've created the output file yet
        bool decompFileCreated = false;

        // Iterate over branches
        for (const auto& branch : args.branches) {
            // Read data from ROOT file
            std::cout << "Reading data for branch '" << branch << "'...\n";
            std::vector<float> data{readVectorFloatBranchData(
                args.dataFile, args.treename, branch
            )};

            // Read entry sizes if we need to write decompressed output
            std::vector<std::size_t> entrySizes;
            if (!args.decompFile.empty()) {
                entrySizes = readBranchEntrySizes(
                    args.dataFile, args.treename, branch
                );
            }

            // Run chunked benchmark
            BenchmarkResult metrics{runChunkedBenchmark(
                *compressor, data, args.chunkSize
            )};

            // Output results as JSON
            std::map<std::string, std::string> compressorConfig = compressor->getConfig();
            nlohmann::json resultJSON = makeBenchmarkJSON(
                args, compressorConfig, metrics, branch
            );
            appendJSONL(args.resultsFile, resultJSON);
            std::cout << "Appended results to " << args.resultsFile << "\n";

            // Write decompressed data to ROOT file if requested
            if (!args.decompFile.empty()) {
                std::vector<std::vector<float>> branchData = reconstructBranchData(
                    metrics.decompressedData, entrySizes
                );

                if (!decompFileCreated) {
                    createTreeWithVectorFloatBranch(
                        args.decompFile, args.treename, branch, branchData
                    );
                    decompFileCreated = true;
                    std::cout << "Created " << args.decompFile << " with branch '" << branch << "'\n";
                } else {
                    insertVectorFloatBranch(
                        args.decompFile, args.treename, branch, branchData
                    );
                    std::cout << "Added branch '" << branch << "' to " << args.decompFile << "\n";
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
