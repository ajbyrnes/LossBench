# Architecture

This document gives a tour of the LossBench codebase for new contributors.
For end-user docs (build, CLI, output format) see [README.md](README.md).

## Project Overview

LossBench is a C++ benchmarking tool for testing error-bounded lossy compression algorithms on data stored in ROOT TTrees (columnar HEP data format). It enables in-memory compression experiments without modifying ROOT, measuring compression ratio, throughput, point-wise error, and distribution-shape distortion metrics.

## Build Commands

```bash
# Configure and build
cmake -S src -B build
cmake --build build -j$(nproc)

# The executable is built at build/lossbench
```

A GoogleTest scaffold lives under `src/tests/` but is currently disabled in the build.

**Dependencies:**
- CMake 3.20+
- C++20 compiler
- ROOT Data Analysis Framework
- nlohmann/json
- zstd
- Optional compressor backends (each enabled iff its dependency is found):
  SZ3, ZFP, ZFPX, SPERR, SZp, MGARD, Tucker, ISABELA

## Running the Tool

```bash
./build/lossbench \
    --inputFile <file.root> \
    --tree <treename> \
    --branches <branch1,branch2,...> \
    --chunkSize <bytes> \
    --compressor <name:opt1=val1,opt2=val2,...> \
    --resultsFile <output.jsonl> \
    [--iterations <n>] \
    [--normalize] \
    [--dataLayout flat|padded2d] \
    [--decompFile <output.root>] \
    [--configFile <tests.json>]
```

Results are appended to the JSONL file if it already exists. Info options: `--list-compressors`, `--compressor-help <name>`, `-h`/`--help`.

## Architecture

```
src/
├── lossbench.cpp          # Entry point: parses args, iterates branches, runs benchmarks
├── compressors/           # Compression implementations (one .hpp/.cpp per backend)
│   ├── Compressor.hpp     # Abstract interface (compress, decompress, configure, setDimensions)
│   ├── factory.{hpp,cpp}  # createCompressor(name); registers all backends behind HAS_* flags
│   ├── ZstdCompressor.*           # Lossless zstd baseline
│   ├── ZstdTruncCompressor.*      # Bit truncation + zstd
│   ├── SZ3Compressor.*            # Error-bounded lossy SZ3 wrapper
│   ├── ZFPCompressor.*            # Lossy floating-point ZFP compression
│   ├── ZFPXCompressor.*           # Experimental ZFPX backend
│   ├── SPERRCompressor.*          # Wavelet-based lossy SPERR
│   ├── SZpCompressor.*            # Error-bounded lossy SZp with OpenMP
│   ├── MGARDCompressor.*          # Multigrid-based MGARD compression
│   ├── TuckerCompressor.*         # Tucker tensor decomposition
│   ├── ISABELACompressor.*        # Windowed B-spline / wavelet compression
│   ├── UniformHistogramCompressor.*   # Custom: uniform-bin histogram + zstd
│   ├── QuantileHistogramCompressor.*  # Custom: quantile-bin histogram + zstd
│   └── QuantileResidualCompressor.*   # Custom: quantile bins + bounded sub-bin residual
├── benchmark/             # Chunked benchmark loop and metric registry
│   ├── benchmark.*                  # runChunkedBenchmark, MetricContext, registerMetric
│   ├── throughput_metrics.cpp       # ratio + compress/decompress throughput
│   ├── error_metrics.cpp            # max/mean abs+rel error, MSE, PSNR
│   └── distribution_metrics.cpp     # KS, Wasserstein, quantile shifts
├── interface/             # CLI parsing and JSONL output
│   └── interface.*        # parseArgs, makeBenchmarkJSON, appendJSONL
├── root-utils/            # ROOT TTree I/O for vector<float> branches
│   └── root-utils.*       # read/insert vector<float> branches, entry sizes
└── tests/                 # GoogleTest scaffold (currently disabled in CMake)
```

**Data flow:**
1. `interface` parses CLI args (and optional `--configFile`) into an `Args` struct holding one or more `TestConfig` entries.
2. Outer loop in `lossbench.cpp` iterates branches; `root-utils` reads each branch as a flat `vector<float>` (plus entry sizes when needed).
3. Inner loop runs every `TestConfig` against the in-memory branch data: `factory` constructs the compressor, `benchmark::runChunkedBenchmark` times the chunked compress/decompress and evaluates every registered metric.
4. `interface::makeBenchmarkJSON` + `appendJSONL` serialize one self-describing row per `(branch, test)` pair.

## Adding a New Compressor

1. Create `NewCompressor.hpp/cpp` in `src/compressors/` implementing the `Compressor` interface.
2. Register it in `src/compressors/factory.cpp` in the `factories` map (gate the include and registration behind a `HAS_*` macro if the backend has an optional dependency).
3. Update `src/compressors/CMakeLists.txt` to include the new source files and propagate any new `HAS_*` flag.

## Current Compressors

- `zstd` — Lossless. Option: `compressionLevel` (1–22).
- `zstd-trunc` — Mantissa-bit truncation + zstd. Options: `compressionLevel`, `truncBits` (0–23).
- `sz3` — Error-bounded lossy (SZ3 framework); configurable error-bound modes.
- `zfp` — Lossy floating-point compression. Options: `mode` (rate/precision/accuracy/reversible), plus matching parameter.
- `zfpx` — Experimental ZFPX backend; same option surface as `zfp`.
- `sperr` — Wavelet-based lossy compression. Options: `mode` (bitrate/psnr/pwe), `quality`, `nthreads`.
- `szp` — Error-bounded lossy with OpenMP. Options: `absErrBound`, `blockSize`.
- `mgard` — Multigrid-based lossy. Options: `mode` (abs/rel), `tolerance`, `smoothness`.
- `tucker` — Tucker tensor decomposition. Option: `epsilon`.
- `isabela` — Windowed B-spline / wavelet. Options: `windowSize`, `ncoefficients`, `errorRate`, `transform`.
- `uniform-histogram` — Custom: uniform-bin histogram + zstd. Options: `nBins`, `zstdLevel`.
- `quantile-histogram` — Custom: quantile-bin histogram + zstd. Options: `nBins`, `edgeCompressionLevel`.
- `quantile-residual` — Custom: quantile bins + bounded sub-bin residual. Options: `nBins`, `residualBits`, `edgeCompressionLevel`.
