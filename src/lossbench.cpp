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

        // Iterate over branches
        for (const auto& branch : args.branches) {
            // Read data from ROOT file
            std::cout << "Reading data for branch '" << branch << "'...\n";
            std::vector<float> data{readVectorFloatBranchData(
                args.dataFile, args.treename, branch
            )};

            // Run benchmark
            CompressionResult compResult{timedCompress(*compressor, data)};
            DecompressionResult decompResult{timedDecompress(
                *compressor, compResult.compressedData
            )};

            // Compute metrics
            BenchmarkResult metrics{computeBenchmarkMetrics(
                data, compResult, decompResult
            )};

            // Output results as JSON
            std::map<std::string, std::string> compressorConfig = compressor->getConfig();
            nlohmann::json resultJSON = makeBenchmarkJSON(
                args, compressorConfig, metrics, compResult, branch
            );
            appendJSONL(args.resultsFile, resultJSON);
            std::cout << "Appended results to " << args.resultsFile << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Use --help for usage information.\n";
        return 1;
    }

    return 0;
}
