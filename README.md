# LossBench: Benchmarking open-source compressors on data stored in columnar ROOT formats

ROOTLess is a research-oriented benchmarking tool for testing error-bounded lossy compression algorithms on data stored in ROOT TTrees*. It allows users to select branches from TTrees, apply a chosen compressor with user-defined configuration, and evaluate compression ratio, speed, and distortion entirely in memory without modifying ROOT. This enables rapid experimentation with new or existing compressors on real HEP data. ROOTLess can also optionally write out compressed data for further inspection or downstream analysis.

The ROOT framework currently provides four lossless compressors (zlib, lzma, lz4, zstd) for writing TTrees. Compression is handled through ROOT’s core I/O functionality, making it nontrivial to add new compressors. If we simply want to understand how a new compressor or configuration performs on typical columnar HEP data, the compression must therefore be performed outside of ROOT. However, reading TTree data still must be done through ROOT—TTrees are compressed per-column (as is typical for columnar formats), so treating an entire TTree as an opaque binary blob would not produce realistic or meaningful results.

*TTrees are currently being deprecated in favor of RNTuples. RNTuples are still a columnar ROOT format, subject to the same challenges for experimenting with new compressors.

## Build

ROOTLess is a CMake project written in C++. It requires a C++ compiler and CMake (exact versions TBD).

Additional external dependencies include:

- [ROOT Data Analysis Framework](https://root.cern/install/)
- [Niels Lohmann JSON libraries](https://github.com/nlohmann/json)
- Compression libraries
  - See [##Compressors]
  - The current build process expects that _all_ supported compressors are present; this will change in the future
 
Building should just require the standard CMake build procedure:

```bash
git clone 
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=<installdir> -S .. -B .
make -j<number of cores on your machine>
make install
```

## Usage

```bash
./ROOTLess --input-file <inFile> --tree-name <treename> --branch-names <branch1,branch2,...>
           --chunk-size <size>
           --compressor <compressor,opt1,opt2,...>
           [--results-file <resFile>]
```

- `--input-file <inFile>`   The path to the `.root` file containing the data to be compressed
- `--tree-name <treename>`  The name of the TTree in `<file>`
- `--branch-names <branch1,branch2,...>`    The branches to read from `<treename>`, as a comma-separated list
- `--chunk-size <size>`     The amount of data to compress at a time, in bytes
- `--compressor <compressor,opt1,opt2,...>` The compressor to use and its arguments, as a comma-separated list
- `[--results-file <resFile>]` Benchmark metrics will be written to `resFile.json`. If `resFile.json` _already exists_, then results will be _appended_ to that file. If this option is not specified, then a filename will be generated using the timestamp of the run.


Results are written in JSON format. This keeps data organized and human readable, for quick inspections. Most analysis and plotting tools are able to parse JSON data. If ROOTLess is told to write benchmark results to a `.json` file that _already_ exists, 

## Examples

ROOTLess is currently designed around testing _individual_ compressor configurations -- that is, each time you run the program, you test _one_ compressor with _one_ particular setting. This avoids having to hard-code loops over each compressor's specific set of options in the program itself.** Iterating over _all_ possible configurations of a compressor can instead be accomplished via scripting. 

For example, the `BitTruncation` compressor takes two arguments: `mantissaBits` and `compressionLevel`. `mantissaBits` can be between `0` and `23`, the number of mantissa bits in a single-precision floating point value. `compressionLevel` can be between `0` and `9`, the compression levels accepted by zlib. The following script uses ROOTLess to test every combination of these settings:

```bash
#!/usr/bin/env bash

# Example script: sweep BitTruncation settings over all combinations of
# mantissaBits = 0..23 and compressionLevel = 0..9.

# Edit these for your setup:
INPUT_FILE="data.root"
TREE_NAME="Events"
BRANCH_NAMES="AnalysisJetsAuxDyn.pt"
CHUNK_SIZE=65536
RESULTS_DIR="results_bittruncation_sweep"

mkdir -p "${RESULTS_DIR}"

for mantissaBits in {0..23}; do
  for compressionLevel in {0..9}; do
    COMPRESSOR_SPEC="BitTruncation,mantissaBits=${mantissaBits},compressionLevel=${compressionLevel}"
    RESULTS_FILE="${RESULTS_DIR}/results_m${mantissaBits}_c${compressionLevel}.json"

    echo "Running ROOTLess with mantissaBits=${mantissaBits}, compressionLevel=${compressionLevel}"

    ./ROOTLess \
      --input-file "${INPUT_FILE}" \
      --tree-name "${TREE_NAME}" \
      --branch-names "${BRANCH_NAMES}" \
      --chunk-size "${CHUNK_SIZE}" \
      --compressor "${COMPRESSOR_SPEC}" \
      --results-file "${RESULTS_FILE}"

  done
done
```

**A significant limitation to this approach is that data needs to be reloaded as we iterate over different compressor configurations. Using the `TTreeReader` class dramatically reduces read time, especially for subsequent reads from the same TTree; however, this behavior is obviously still not desirable.

## Compressors

- Custom bit truncation compressor
  - Performs bit truncation before losslessly compressing with [zlib](https://github.com/madler/zlib)
- [SZ3: A Modular Error-bounded Lossy Compression Framework for Scientific Datasets](https://github.com/szcompressor/SZ3)

## Metrics and Reporting
Currently, ROOTLess collects and reports all of the following information:
  - Compression ratio (original data bytes / compressed data bytes)
  - Compression throughput (MB/s)
  - Decompression throughput (MB/s)
  - Min/max/mean pointwise absolute error
  - Min/max/mean pointwise relative error
  - Mean-squared error (MSE)
  - Peak signal-to-noise ratio (PSNR)

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

Currently the `tests/` directory contains executables which were written to test individual methods as they were being developed.
However, these do not use any unit testing framework and have not been automated via `ctest`. 
Actual unit testing with automation is among the next significant TODO items.

## TODOs

- Decide minimum C++ and CMake required versions
  - Remove dependency on <format>; it's so lovely but makes C++20 a hard requirement, which has implications about ROOT installations
  - Maybe we could just do that thing people have been doing where they include <format> as a separate library?
- Implement unit and integrated testing
- Change compressor configuration parsing to make it easier to add new compressors
- List supported options for each compressor
