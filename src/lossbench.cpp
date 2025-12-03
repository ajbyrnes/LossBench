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
}
