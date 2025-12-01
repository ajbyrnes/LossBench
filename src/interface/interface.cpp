#include <format>
#include <iostream>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "interface.hpp"

static std::vector<std::string> tokenize(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::size_t start = 0;
    while (start < str.size()) {
        const std::size_t pos = str.find(delimiter, start);
        if (pos == std::string::npos) {
            tokens.push_back(str.substr(start));
            break;
        }
        tokens.push_back(str.substr(start, pos - start));
        start = pos + 1;
    }
    return tokens;
}

static std::map<std::string, std::string> parseCompressorConfig(const std::string& configList) {
    std::map<std::string, std::string> configMap;
    if (configList.empty()) {
        return configMap;
    }

    const auto configTokens = tokenize(configList, ',');
    for (const auto& item : configTokens) {
        const auto separator = item.find('=');
        if (separator == std::string::npos || separator == 0 || separator == item.size() - 1) {
            throw std::runtime_error("Compressor option must be key=value; saw '" + item + "'");
        }
        configMap[item.substr(0, separator)] = item.substr(separator + 1);
    }
    return configMap;
}

Args parseArgs(int argc, char* argv[]) {
    Args args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--inputFile" && i + 1 < argc) {
            // --inputFile <file>
            args.dataFile = argv[++i];
        } else if (arg == "--tree" && i + 1 < argc) {
            // --tree <name>
            args.treename = argv[++i];
        } else if (arg == "--branches" && i + 1 < argc) {
            // --branches <branch1,branch2,...>
            std::vector<std::string> branches = tokenize(argv[++i], ',');
            args.branches = std::move(branches);
        } else if (arg == "--chunkSize" && i + 1 < argc) {
            // --chunkSize <number>
            args.chunkSize = std::stoul(argv[++i]);
        } else if (arg == "--compressor" && i + 1 < argc) {
            // --compressor <name[:opt1=val,opt2=val,...]>
            std::string compressorConfig = argv[++i];

            const auto colon = compressorConfig.find(':');
            if (colon == std::string::npos) {
                args.compressor = compressorConfig;
                args.compressionOptions.clear();
            } else {
                args.compressor = compressorConfig.substr(0, colon);
                const std::string opts = compressorConfig.substr(colon + 1);
                args.compressionOptions = parseCompressorConfig(opts);
            }
        } else if (arg == "--resultsFile" && i + 1 < argc) {
            // --resultsFile <file>
            args.resultsFile = argv[++i];
        } else if (arg == "--decompFile" && i + 1 < argc) {
            // [--decompFile <file>]
            args.decompFile = argv[++i];
        } else {
            throw std::runtime_error("Unknown or incomplete argument: " + arg);
        }
    }

    if (args.dataFile.empty() || args.treename.empty() ||
        args.branches.empty() || args.chunkSize == 0 ||
        args.compressor.empty()) {
        printUsage();
        throw std::runtime_error("Missing required arguments");
    }

    return args;
}

void printUsage() {
    std::cout << "Usage: lossbench "
                 "--inputFile <file> "
                 "--tree <name> "
                 "--branches <branch1,branch2,...> "
                 "--chunkSize <number> "
                 "--compressor <name[:opt1=val,opt2=val,...]> "
                 "[--resultsFile <file>] "
                 "[--decompFile <file>]"
                 "\n";
}

void printArgs(const Args& args) {
    std::cout << "---------- Command-Line Arguments ----------\n";
    std::cout << "Input file: " << args.dataFile << "\n";
    std::cout << "Tree name: " << args.treename << "\n";
    std::cout << "Branches:\n";
    for (const auto& branch : args.branches) {
        std::cout << "  " << branch << "\n";
    }
    std::cout << "Chunk size: " << args.chunkSize << "\n";
    std::cout << "Compressor: " << args.compressor << "\n";
    std::cout << "Compression options:\n";
    for (const auto& [key, value] : args.compressionOptions) {
        std::cout << "  " << key << ": " << value << "\n";
    }
    std::cout << "Results file: " << args.resultsFile << "\n";
    if (!args.decompFile.empty()) {
        std::cout << "Decompressed output: " << args.decompFile << "\n";
    } else {
        std::cout << "Decompressed output: (not written)\n";
    }
    std::cout << "--------------------------------------------\n";
}

nlohmann::json makeBenchmarkJSON(
    const Args& args,
    const BenchmarkResult& metrics,
    const CompressionResult& comp,
    const DecompressionResult& decomp)
{
    nlohmann::json j;

    // Echo input configuration
    j["config"] = {
        {"input_file", args.dataFile},
        {"tree", args.treename},
        {"branches", args.branches},
        {"chunk_size", args.chunkSize},
        {"compressor", args.compressor},
        {"compressor_options", args.compressionOptions},
        {"results_file", args.resultsFile},
        {"decomp_file", args.decompFile}
    };

    // Metrics and sizes
    j["results"] = {
        {"original_size_bytes", comp.originalSizeBytes},
        {"compressed_size_bytes", comp.compressedSizeBytes},
        {"compression_ratio", metrics.compressionRatio},
        {"compression_throughput_mbps", metrics.compressionThroughputMbps},
        {"decompression_throughput_mbps", metrics.decompressionThroughputMbps},
        {"abs_error_max", metrics.absErrorMax},
        {"abs_error_avg", metrics.absErrorAvg},
        {"rel_error_max", metrics.relErrorMax},
        {"rel_error_avg", metrics.relErrorAvg},
        {"mse", metrics.MSE},
        {"psnr", metrics.PSNR}
    };

    return j;
}

void appendJSONL(const std::string& filepath, const nlohmann::json& entry) {
    std::ofstream out(filepath, std::ios::app);
    if (!out) {
        throw std::runtime_error("Failed to open JSONL file for append: " + filepath);
    }
    out << entry.dump() << '\n';
}
