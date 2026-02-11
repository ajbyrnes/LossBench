# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LossBench is a C++ benchmarking tool for testing error-bounded lossy compression algorithms on data stored in ROOT TTrees (columnar HEP data format). It enables in-memory compression experiments without modifying ROOT, measuring compression ratio, throughput, and distortion metrics.

## Build Commands

```bash
# Configure and build
cmake -S src -B build
cmake --build build -j$(nproc)

# The executable is built at build/lossbench
```

**Dependencies:**
- CMake 3.20+
- C++20 compiler
- ROOT Data Analysis Framework
- nlohmann/json
- zstd
- SZ3 (optional)
- ZFP (optional)
- SPERR (optional)
- SZp (optional)

## Running the Tool

```bash
./build/lossbench \
    --inputFile <file.root> \
    --tree <treename> \
    --branches <branch1,branch2,...> \
    --chunkSize <bytes> \
    --compressor <name:opt1=val1,opt2=val2,...> \
    --resultsFile <output.jsonl> \
    [--decompFile <output.root>]
```

Results are appended to the JSONL file if it already exists.

## Architecture

```
src/
├── lossbench.cpp          # Entry point: parses args, iterates branches, runs benchmarks
├── compressors/           # Compression implementations
│   ├── Compressor.hpp     # Abstract interface (compress, decompress, configure)
│   ├── factory.cpp        # Factory function createCompressor(name)
│   ├── ZstdCompressor.*   # Lossless zstd wrapper (option: compressionLevel)
│   ├── ZstdTruncCompressor.*  # Bit truncation + zstd (options: compressionLevel, truncBits)
│   ├── SZ3Compressor.*    # Error-bounded lossy SZ3 wrapper
│   ├── ZFPCompressor.*    # Lossy floating-point ZFP compression
│   ├── SPERRCompressor.*  # Wavelet-based lossy SPERR compression
│   └── SZpCompressor.*   # Error-bounded lossy SZp compression with OpenMP
├── benchmark/             # Timing and metrics computation
│   └── benchmark.*        # timedCompress, timedDecompress, computeBenchmarkMetrics
├── interface/             # CLI parsing and JSON output
│   └── interface.*        # parseArgs, makeBenchmarkJSON, appendJSONL
└── root-utils/            # ROOT file I/O utilities
    └── root-utils.*       # readVectorFloatBranchData, insertVectorFloatBranch
```

**Data flow:**
1. `interface` parses CLI args into `Args` struct
2. `factory` creates the appropriate `Compressor` instance
3. `root-utils` reads branch data from ROOT TTrees as `vector<float>`
4. `benchmark` runs timed compression/decompression and computes metrics
5. `interface` serializes results to JSONL

## Adding a New Compressor

1. Create `NewCompressor.hpp/cpp` in `src/compressors/` implementing the `Compressor` interface
2. Register it in `src/compressors/factory.cpp` in the `kFactories` map
3. Update `src/compressors/CMakeLists.txt` to include the new source files

## Current Compressors

- `zstd` - Lossless, option: `compressionLevel` (1-22, default 3)
- `zstd-trunc` - Bit truncation + zstd, options: `compressionLevel` (1-22), `truncBits` (0-23)
- `sz3` - Error-bounded lossy (SZ3 framework), configurable error bounds and modes
- `zfp` - Lossy floating-point compression, options: `mode` (rate/precision/accuracy/reversible), `tolerance`
- `sperr` - Wavelet-based lossy compression, options: `mode` (bpp/psnr/pwe), `quality`
- `szp` - Error-bounded lossy compression with OpenMP, options: `absErrBound` (default 1e-6), `blockSize` (default 128)
