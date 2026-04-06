#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

#include <nlohmann/json.hpp>

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

// Load tests (and optional global args) from a JSON config file.
// Config file format:
//   {
//     "inputFile":  "<path>",          // optional if provided on CLI
//     "tree":       "<name>",          // optional if provided on CLI
//     "branches":   ["b1", "b2"],      // optional if provided on CLI
//     "resultsFile":"<path>",          // optional if provided on CLI
//     "tests": [
//       {
//         "compressor": "<name>",
//         "options":    { "key": "value" },   // optional
//         "chunkSize":  <bytes>,
//         "iterations": <n>,                  // optional, default 1
//         "decompFile": "<path>"              // optional
//       }
//     ]
//   }
static Args parseConfigFile(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f) {
        throw std::runtime_error("Cannot open config file: " + filepath);
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(f);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("Failed to parse config file '" + filepath + "': " + e.what());
    }

    Args args;
    if (j.contains("inputFile"))   args.dataFile   = j["inputFile"].get<std::string>();
    if (j.contains("tree"))        args.treename   = j["tree"].get<std::string>();
    if (j.contains("branches"))    args.branches   = j["branches"].get<std::vector<std::string>>();
    if (j.contains("resultsFile")) args.resultsFile = j["resultsFile"].get<std::string>();

    if (!j.contains("tests") || !j["tests"].is_array()) {
        throw std::runtime_error("Config file must contain a 'tests' array");
    }
    if (j["tests"].empty()) {
        throw std::runtime_error("Config file 'tests' array must not be empty");
    }

    for (const auto& t : j["tests"]) {
        if (!t.contains("compressor")) {
            throw std::runtime_error("Each test must specify a 'compressor'");
        }
        if (!t.contains("chunkSize")) {
            throw std::runtime_error("Each test must specify a 'chunkSize'");
        }

        TestConfig test;
        test.compressor = t["compressor"].get<std::string>();
        test.chunkSize  = t["chunkSize"].get<std::size_t>();

        if (t.contains("options")) {
            test.compressionOptions = t["options"].get<std::map<std::string, std::string>>();
        }
        if (t.contains("iterations")) {
            test.iterations = t["iterations"].get<std::size_t>();
        }
        if (t.contains("decompFile")) {
            test.decompFile = t["decompFile"].get<std::string>();
        }

        args.tests.push_back(std::move(test));
    }

    return args;
}

Args parseArgs(int argc, char* argv[]) {
    Args args;
    std::string configFilePath;
    TestConfig singleTest;
    bool hasSingleTest = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--inputFile" && i + 1 < argc) {
            args.dataFile = argv[++i];
        } else if (arg == "--tree" && i + 1 < argc) {
            args.treename = argv[++i];
        } else if (arg == "--branches" && i + 1 < argc) {
            args.branches = tokenize(argv[++i], ',');
        } else if (arg == "--chunkSize" && i + 1 < argc) {
            singleTest.chunkSize = std::stoul(argv[++i]);
        } else if (arg == "--compressor" && i + 1 < argc) {
            // --compressor <name[:opt1=val,opt2=val,...]>
            std::string compressorConfig = argv[++i];
            const auto colon = compressorConfig.find(':');
            if (colon == std::string::npos) {
                singleTest.compressor = compressorConfig;
                singleTest.compressionOptions.clear();
            } else {
                singleTest.compressor = compressorConfig.substr(0, colon);
                singleTest.compressionOptions = parseCompressorConfig(compressorConfig.substr(colon + 1));
            }
            hasSingleTest = true;
        } else if (arg == "--resultsFile" && i + 1 < argc) {
            args.resultsFile = argv[++i];
        } else if (arg == "--decompFile" && i + 1 < argc) {
            singleTest.decompFile = argv[++i];
        } else if (arg == "--iterations" && i + 1 < argc) {
            singleTest.iterations = std::stoul(argv[++i]);
        } else if (arg == "--configFile" && i + 1 < argc) {
            configFilePath = argv[++i];
        } else {
            throw std::runtime_error("Unknown or incomplete argument: " + arg);
        }
    }

    if (!configFilePath.empty()) {
        if (hasSingleTest) {
            throw std::runtime_error("--compressor and --configFile are mutually exclusive");
        }
        // Load config file; CLI global args override anything in the file.
        Args fileArgs = parseConfigFile(configFilePath);
        if (args.dataFile.empty())    args.dataFile    = fileArgs.dataFile;
        if (args.treename.empty())    args.treename    = fileArgs.treename;
        if (args.branches.empty())    args.branches    = fileArgs.branches;
        if (args.resultsFile.empty()) args.resultsFile = fileArgs.resultsFile;
        args.tests = std::move(fileArgs.tests);
    } else {
        // Single-test CLI mode: validate and wrap in tests vector.
        if (!hasSingleTest || singleTest.chunkSize == 0) {
            printUsage();
            throw std::runtime_error("Missing required arguments");
        }
        args.tests.push_back(std::move(singleTest));
    }

    if (args.dataFile.empty() || args.treename.empty() ||
        args.branches.empty() || args.tests.empty()) {
        printUsage();
        throw std::runtime_error("Missing required arguments");
    }

    return args;
}

void printUsage() {
    std::cout <<
        "Usage (single test):\n"
        "  lossbench"
        " --inputFile <file>"
        " --tree <name>"
        " --branches <b1,b2,...>"
        " --chunkSize <bytes>"
        " --compressor <name[:opt=val,...]>"
        " [--resultsFile <file>]"
        " [--decompFile <file>]"
        " [--iterations <n>]\n"
        "\n"
        "Usage (multiple tests from config file):\n"
        "  lossbench"
        " [--inputFile <file>]"
        " [--tree <name>]"
        " [--branches <b1,b2,...>]"
        " [--resultsFile <file>]"
        " --configFile <config.json>\n";
}

void printArgs(const Args& args) {
    std::cout << "---------- Configuration ----------\n";
    std::cout << "Input file:   " << args.dataFile << "\n";
    std::cout << "Tree name:    " << args.treename << "\n";
    std::cout << "Results file: " << (args.resultsFile.empty() ? "(none)" : args.resultsFile) << "\n";
    std::cout << "Branches:\n";
    for (const auto& branch : args.branches) {
        std::cout << "  " << branch << "\n";
    }
    std::cout << "Tests (" << args.tests.size() << "):\n";
    for (std::size_t i = 0; i < args.tests.size(); ++i) {
        const auto& test = args.tests[i];
        std::cout << "  [" << i << "] compressor=" << test.compressor;
        if (!test.compressionOptions.empty()) {
            std::cout << " options={";
            bool first = true;
            for (const auto& [k, v] : test.compressionOptions) {
                if (!first) std::cout << ", ";
                std::cout << k << "=" << v;
                first = false;
            }
            std::cout << "}";
        }
        std::cout << " chunkSize=" << test.chunkSize;
        std::cout << " iterations=" << test.iterations;
        if (!test.decompFile.empty()) {
            std::cout << " decompFile=" << test.decompFile;
        }
        std::cout << "\n";
    }
    std::cout << "-----------------------------------\n";
}

static std::string getHost() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    } else {
        return "unknown_host";
    }
}

static std::string getTimestamp(bool filenameSafe=false) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::tm tm = *std::localtime(&in_time_t);
    char buffer[100];

    if (filenameSafe) {
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &tm);
    } else {
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    }

    return std::string(buffer);
}

nlohmann::json makeBenchmarkJSON(
    const Args& args,
    const TestConfig& test,
    const std::map<std::string, std::string>& compressorConfig,
    const BenchmarkResult& metrics,
    std::string branch)
{
    nlohmann::json j;

    // System info
    j["system"] = {
        {"host", getHost()},
        {"timestamp", getTimestamp()}
    };

    // Echo input configuration
    j["config"] = {
        {"input_file", args.dataFile},
        {"tree", args.treename},
        {"branches", branch},
        {"chunk_size", test.chunkSize},
        {"iterations", test.iterations},
        {"compressor", test.compressor},
        {"compressor_config", compressorConfig},
        {"results_file", args.resultsFile},
        {"decomp_file", test.decompFile}
    };

    // Metrics and sizes
    j["results"] = {
        {"original_size_bytes", metrics.originalSizeBytes},
        {"compressed_size_bytes", metrics.compressedSizeBytes},
        {"compression_ratio", metrics.compressionRatio},
        {"compression_throughput_mbps", metrics.compressionThroughputMbps},
        {"decompression_throughput_mbps", metrics.decompressionThroughputMbps},
        {"abs_error_max", metrics.absErrorMax},
        {"abs_error_avg", metrics.absErrorAvg},
        {"rel_error_max", metrics.relErrorMax},
        {"rel_error_avg", metrics.relErrorAvg},
        {"mse", metrics.MSE},
        {"psnr", metrics.PSNR},
        {"ks_statistic", metrics.ksStatistic},
        {"ks_p_value", metrics.ksPValue},
        {"wasserstein_distance", metrics.wassersteinDistance},
        {"q5_shift", metrics.q5Shift},
        {"q50_shift", metrics.q50Shift},
        {"q95_shift", metrics.q95Shift},
        {"q99_shift", metrics.q99Shift}
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