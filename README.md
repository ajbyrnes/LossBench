# LossBench: Benchmarking open-source compressors on data stored in columnar ROOT formats

LossBench is a research-oriented benchmarking tool for testing error-bounded lossy compression algorithms on data stored in ROOT TTrees*. It allows users to select branches from TTrees, apply a chosen compressor with user-defined configuration, and evaluate compression ratio, speed, and distortion entirely in memory without modifying ROOT. This enables rapid experimentation with new or existing compressors on real HEP data. LossBench can also optionally write out compressed data for further inspection or downstream analysis.

The ROOT framework currently provides four lossless compressors (zlib, lzma, lz4, zstd) for writing TTrees. Compression is handled through ROOT’s core I/O functionality, making it nontrivial to add new compressors. If we simply want to understand how a new compressor or configuration performs on typical columnar HEP data, the compression must therefore be performed outside of ROOT. However, reading TTree data still must be done through ROOT—TTrees are compressed per-column (as is typical for columnar formats), so treating an entire TTree as an opaque binary blob would not produce realistic or meaningful results.

*TTrees are currently being deprecated in favor of RNTuples. RNTuples are still a columnar ROOT format, subject to the same challenges for experimenting with new compressors.

## Build

LossBench is a CMake project written in C++. It requires a C++ compiler and CMake (exact versions TBD).

Additional external dependencies include:

- [ROOT Data Analysis Framework](https://root.cern/install/)
- [Niels Lohmann JSON libraries](https://github.com/nlohmann/json)
- Compression libraries
  - See [##Compressors]
  - The current build process expects that _all_ supported compressors are present; this will change in the future
 
Building should just require the standard CMake build procedure:

```bash
git clone https://github.com/ajbyrnes/LossBench.git
mkdir build
cmake -S LossBench -B build
cmake --build build -j<numThreads>
```

## Usage

```bash
./lossbench --inputFile <inputFile> --tree <treename> --branches <branch1,branch2,...>
            --chunkSize <size>
            --compressor <compressor:opt1=val1,opt2=val2,...>
            --resultsFile <resultsFile>
            [--decompFile <decompFile>]
```

- `--inputFile <inputFile>`   The path to the `.root` file containing the data to be compressed
- `--tree <treename>`  The name of the TTree in `<inputFile>`
- `--branches <branch1,branch2,...>`    The branches to read from `<treename>`, as a comma-separated list
- `--chunkSize <size>`     The amount of data to compress at a time, in bytes
- `--compressor <compressor:opt1=1,opt2=val2,...>` The compressor to use and its arguments, as a comma-separated list of `key=value` items
- `--resultsFile <resultsFile>` Benchmark metrics will be written to `resultsFile.jsonl`. If `resultsFile.jsonl` _already exists_, then results will be _appended_ to that file.
- `[--decompFile <decompFile>]` Decompressed data will be written to `decompFile.root`. If `--decompFile` is not specified, data is not written.


Results are written in JSONL format. This keeps data organized and human readable, for quick inspections. Most analysis and plotting tools are able to parse JSONL data. If LossBench is told to write benchmark results to a `.jsonl` file that _already_ exists, 

## Examples

See the `examples` directory for a more walkthough-style example of using LossBench.

LossBench is currently designed around testing _individual_ compressor configurations -- that is, each time you run the program, you test _one_ compressor with _one_ particular setting. This avoids having to hard-code loops over each compressor's specific set of options in the program itself.** Iterating over _all_ possible configurations of a compressor can instead be accomplished via scripting. 

For example, the `zlib` compressor

```bash
#!/bin/bash
# Benchmark script for zlib compression with varying compression levels.

# Create a timestamped results directory to store output files:
#    results.jsonl -- results of running the benchmarks, in JSONL format
#    benchmark.log -- logging statements output by the benchmarking program, in plain text
# The timestamp uniquely identifies the run and prevents overwriting previous results
timestamp=$(date +"%Y%m%d_%H%M%S")

results_dir="results_zlib_$timestamp"
mkdir -p "$results_dir"

results_file="$results_dir/results.jsonl"
log_file="$results_dir/benchmark.log"

# Benchmark parameters
input_file="jets.root"
tree_name="CollectionTree"
branches="AnalysisJetsAuxDyn.pt,AnalysisJetsAuxDyn.eta,AnalysisJetsAuxDyn.phi,AnalysisJetsAuxDyn.m"
chunk_size=32768

# Run benchmark with zlib for varying compression levels
for compression_level in {1..9}
do
    echo "Running benchmark with zlib compression level $compression_level..." >> "$log_file" 2>&1

    ./build/lossbench \
        --inputFile "$input_file" \
        --tree "$tree_name" \
        --branches "$branches" \
        --chunkSize "$chunk_size" \
        --compressor "zlib:compressionLevel=$compression_level" \
        --resultsFile "$results_file" >> "$log_file" 2>&1

    echo "Benchmark with zlib compression level $compression_level completed." >> "$log_file" 2>&1
done

```

**A significant limitation to this approach is that data needs to be reloaded as we iterate over different compressor configurations. Using the `TTreeReader` class dramatically reduces read time, especially for subsequent reads from the same TTree; however, this behavior is obviously still not desirable.

## Compressors

- `zlib` -- Wrapper around [zlib](https://github.com/madler/zlib)
- Custom bit truncation compressor
  - Performs bit truncation before losslessly compressing with [zlib](https://github.com/madler/zlib)
- `sz3` -- Wrapper around [SZ3: A Modular Error-bounded Lossy Compression Framework for Scientific Datasets](https://github.com/szcompressor/SZ3)
  - SZ3 has a dependency on [zstd](https://github.com/facebook/zstd)

## Metrics and Reporting

Currently, LossBench collects and reports all of the following information:
  - Compression ratio (original data bytes / compressed data bytes)
  - Compression throughput (MB/s)
  - Decompression throughput (MB/s)
  - Max/mean pointwise absolute error
  - Max/mean pointwise relative error
  - Mean-squared error (MSE)
  - Peak signal-to-noise ratio (PSNR)

The following information about JSON output is outdated. LossBench now reports metrics with JSONL, and this section needs to be updated.
However, the example JSON output is still close to what you will see in the `.jsonl` output.

The JSON results also contain the settings used for each run, so these do not need to be recorded separately.

Example JSON output:

```JSON
{
  "args": {
    "branch": "AnalysisJetsAuxDyn.pt",
    "chunkSize": 16384,
    "compressionOptions": {
      "compressionLevel": "5",
      "mantissaBits": "8"
    },
    "compressor": "BitTruncation",
    "dataFile": "DAOD_PHYSLITE.37019878._000009.pool.root.1",
    "decompFile": "",
    "treename": "CollectionTree",
    "writeDecompressed": false
  },
  "host": "Niamh",
  "results": {
    "compressionRatio": 2.3435992143860864,
    "compressionThroughputMBps": 3.149040663090863,
    "decompressionThroughputMBps": 128.13904801369063,
    "maxRelError": 0.19491989937883336,
    "meanRelError": 0.06987837935555764,
    "minRelError": 0.0,
  },
  "timestamp": "2025-09-17_16-08-55"
}
```

## Tests

TODO

## TODOs

- Add way to list supported compressors
- Add way to list options for supported compressors
- Add test suites with GoogleTest
- Authorship
- Liscense?
