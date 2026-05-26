# LossBench: Benchmarking open-source compressors on data stored in columnar ROOT formats

LossBench is a research-oriented benchmarking tool for testing error-bounded lossy compression algorithms on data stored in ROOT TTrees*. It allows users to select branches from TTrees, apply a chosen compressor with user-defined configuration, and evaluate compression ratio, speed, and distortion entirely in memory without modifying ROOT. This enables rapid experimentation with new or existing compressors on real HEP data. LossBench can also optionally write out compressed data for further inspection or downstream analysis.

The ROOT framework currently provides four lossless compressors (zlib, lzma, lz4, zstd) for writing TTrees. Compression is handled through ROOT’s core I/O functionality, making it nontrivial to add new compressors. If we simply want to understand how a new compressor or configuration performs on typical columnar HEP data, the compression must therefore be performed outside of ROOT. However, reading TTree data still must be done through ROOT—TTrees are compressed per-column (as is typical for columnar formats), so treating an entire TTree as an opaque binary blob would not produce realistic or meaningful results.

*TTrees are currently being deprecated in favor of RNTuples. RNTuples are still a columnar ROOT format, subject to the same challenges for experimenting with new compressors.

## Build

LossBench is a CMake project written in C++20. Build requirements:

- CMake 3.20+
- A C++20 compiler
- [ROOT Data Analysis Framework](https://root.cern/install/)
- [nlohmann/json](https://github.com/nlohmann/json)
- [zstd](https://github.com/facebook/zstd)

Optional compressor backends — each is enabled when its dependency is found
at configure time and skipped otherwise:

- [SZ3](https://github.com/szcompressor/SZ3)
- [ZFP](https://computing.llnl.gov/projects/zfp)
- [SPERR](https://github.com/NCAR/SPERR)
- [SZp](https://github.com/szcompressor/SZp)
- [MGARD](https://github.com/CODARcode/MGARD)
- Tucker, ISABELA, ZFPX

Standard CMake build procedure:

```bash
git clone https://github.com/ajbyrnes/LossBench.git
cd LossBench
cmake -S src -B build
cmake --build build -j$(nproc)
```

The executable is produced at `build/lossbench`. A GoogleTest suite is
scaffolded under `src/tests/` but is currently disabled in the CMake
configuration; re-enabling and rounding it out is on the TODO list.

## Usage

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

Required:

- `--inputFile <file.root>` — input `.root` file containing the data to be compressed.
- `--tree <treename>` — name of the TTree in `--inputFile`.
- `--branches <b1,b2,...>` — comma-separated list of branches to read.
- `--chunkSize <bytes>` — chunk size in bytes; 0 means "one chunk".
- `--compressor <name:opt1=val1,...>` — compressor name plus a comma-separated
  list of `key=value` options.
- `--resultsFile <output.jsonl>` — JSONL output path; rows are appended if
  the file already exists.

Optional:

- `--iterations <n>` — repeat each compress/decompress run `n` times and
  average the timing.
- `--normalize` — normalize each chunk before compression and undo the
  normalization after decompression.
- `--dataLayout flat|padded2d` — treat per-event vectors as a flat 1-D
  stream (default) or zero-pad them into a 2-D row-major array so
  multi-dimensional backends (ZFP, SZ3, SPERR) can use their native APIs.
- `--decompFile <output.root>` — also write the decompressed data into a
  parallel ROOT file. Omit to skip.
- `--configFile <tests.json>` — queue many `(compressor, options, chunk,
  layout)` test configurations in one invocation; each is run against
  every selected branch.

Info options:

- `--list-compressors` — print the compressors available in this build.
- `--compressor-help <name>` — print the option surface of a specific compressor.
- `-h`, `--help` — print top-level usage.

Results are written in [JSON Lines](https://jsonlines.org/) format: one
self-describing JSON object per benchmark run. Each row records the
inputs, the compressor configuration, and the measured metrics, so rows
can be filtered, joined, or plotted without external bookkeeping.

## Examples

See the `examples` directory for walkthrough-style examples of using LossBench.

Each LossBench invocation can run multiple `(compressor, options, chunk,
layout)` tests against the same branches. The simplest pattern is one
test per invocation, driven from a shell loop over the parameter you want
to sweep:

```bash
#!/bin/bash
# Sweep zstd compression levels against four jet branches.

timestamp=$(date +"%Y%m%d_%H%M%S")
results_dir="results_zstd_$timestamp"
mkdir -p "$results_dir"

results_file="$results_dir/results.jsonl"
log_file="$results_dir/benchmark.log"

input_file="jets.root"
tree_name="CollectionTree"
branches="AnalysisJetsAuxDyn.pt,AnalysisJetsAuxDyn.eta,AnalysisJetsAuxDyn.phi,AnalysisJetsAuxDyn.m"
chunk_size=32768

for level in {1..9}; do
    ./build/lossbench \
        --inputFile "$input_file" \
        --tree "$tree_name" \
        --branches "$branches" \
        --chunkSize "$chunk_size" \
        --compressor "zstd:compressionLevel=$level" \
        --resultsFile "$results_file" >> "$log_file" 2>&1
done
```

The shell-loop pattern reloads the branch data on every invocation. For
large sweeps, prefer `--configFile <tests.json>`: all queued tests are
run against each branch's in-memory buffer, so the ROOT data is read
once and reused.

## Compressors

LossBench ships with wrappers around several third-party compression
libraries plus a few custom histogram-style baselines. Which backends are
actually available in a given build depends on which optional dependencies
were found at CMake configure time; run

```bash
./build/lossbench --list-compressors
```

for the live list and

```bash
./build/lossbench --compressor-help <name>
```

for per-compressor options.

**Lossless / near-lossless**

- `zstd` — Direct wrapper around [zstd](https://github.com/facebook/zstd).
  Option: `compressionLevel` (1–22).
- `zstd-trunc` — Mantissa-bit truncation followed by zstd. Options:
  `compressionLevel`, `truncBits` (0–23).

**Error-bounded lossy (third-party)**

- `sz3` — [SZ3 modular error-bounded lossy compression framework](https://github.com/szcompressor/SZ3).
- `zfp` — [ZFP floating-point compression](https://computing.llnl.gov/projects/zfp).
  Options: `mode` (`rate` | `precision` | `accuracy` | `reversible`), plus
  the matching parameter.
- `zfpx` — Experimental ZFPX backend; same option surface as `zfp`.
- `sperr` — [SPERR wavelet-based compression](https://github.com/NCAR/SPERR).
  Options: `mode` (`bitrate` | `psnr` | `pwe`), `quality`, `nthreads`.
- `szp` — [SZp absolute-error-bounded compression](https://github.com/szcompressor/SZp)
  with OpenMP. Options: `absErrBound`, `blockSize`.
- `mgard` — [MGARD multigrid-based compression](https://github.com/CODARcode/MGARD).
  Options: `mode` (`abs` | `rel`), `tolerance`, `smoothness`.
- `tucker` — Tucker tensor decomposition. Option: `epsilon`.
- `isabela` — [ISABELA windowed B-spline / wavelet compression](https://users.nccs.gov/~scampbel/isabela/).
  Options: `windowSize`, `ncoefficients`, `errorRate`, `transform`.

**Custom histogram baselines**

- `uniform-histogram` — Equal-width histogram quantization + zstd on the
  bin indices. Options: `nBins`, `zstdLevel`.
- `quantile-histogram` — Quantile-spaced histogram + zstd on indices and
  bin edges. Options: `nBins`, `edgeCompressionLevel`.
- `quantile-residual` — Quantile histogram plus a bounded fixed-width
  residual inside each bin. Options: `nBins`, `residualBits`,
  `edgeCompressionLevel`.

## Metrics and Reporting

Each benchmark run produces one JSONL row containing the full
configuration, system metadata, and the following metrics:

**Throughput / size**

- `compression_ratio` — original bytes / compressed bytes
- `original_size_bytes`, `compressed_size_bytes`
- `compression_throughput_mbps`, `decompression_throughput_mbps`

**Point-wise error**

- `abs_error_max`, `abs_error_avg` — max and mean absolute error
- `rel_error_max`, `rel_error_avg` — max and mean relative error
- `mse` — mean squared error
- `psnr` — peak signal-to-noise ratio (dB)

**Distribution-shape**

- `ks_statistic`, `ks_p_value` — Kolmogorov–Smirnov two-sample test
- `wasserstein_distance` — 1-D Wasserstein (earth mover's) distance
- `q5_shift`, `q50_shift`, `q95_shift`, `q99_shift` — shift of selected
  quantiles between original and reconstructed distributions

Each row is fully self-describing — the configuration that produced it
is recorded inline, so no separate run log is needed.

Example JSONL row (one line, pretty-printed here for readability):

```json
{
  "config": {
    "input_file": "DAOD_PHYSLITE.37019878._000009.pool.root.1",
    "tree": "CollectionTree",
    "branches": "AnalysisSiHitElectronsAuxDyn.pt",
    "chunk_size": 16384,
    "iterations": 5,
    "normalize": false,
    "compressor": "sperr",
    "compressor_config": {
      "mode": "bitrate",
      "quality": "2.000000",
      "nthreads": "0"
    },
    "results_file": "results.jsonl",
    "decomp_file": ""
  },
  "results": {
    "original_size_bytes": 24536,
    "compressed_size_bytes": 1606,
    "compression_ratio": 15.277709,
    "compression_throughput_mbps": 38.46,
    "decompression_throughput_mbps": 87.88,
    "abs_error_max": 5445.494,
    "abs_error_avg": 1801.335,
    "rel_error_max": 3.267,
    "rel_error_avg": 0.470,
    "mse": 4968646.0,
    "psnr": 38.18,
    "ks_statistic": 0.476,
    "ks_p_value": 0.0,
    "wasserstein_distance": 1537.51,
    "q5_shift": 933.92,
    "q50_shift": 2689.57,
    "q95_shift": 1537.20,
    "q99_shift": 129.41
  },
  "system": {
    "host": "x1000c0s2b0n1",
    "timestamp": "2026-04-09 13:19:08"
  }
}
```

## Tests

A GoogleTest suite is scaffolded under `src/tests/` but is currently
disabled in the build. Re-enabling it and expanding coverage is tracked
in the TODO list.

## Authors

LossBench is developed by Amy J. Byrnes (<ajbyrne2@uic.edu>), with
support from the Computational and Computer Science for the Physics
Frontier (C2theP2) fellowship.

## License

License pending institutional review at the University of Illinois Chicago;
a permissive license (BSD 3-Clause or equivalent) is planned. In the
interim, conference attendees and academic researchers may download, build,
and evaluate the software for non-commercial purposes. For any other use,
contact <ajbyrne2@uic.edu>. See [LICENSE](LICENSE) for the full notice.

## TODOs

- Re-enable and expand the GoogleTest suite
