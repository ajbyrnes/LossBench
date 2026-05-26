# Extending LossBench

This guide walks through the two most common ways to extend LossBench:
adding a new compressor backend, and adding a new benchmark metric.

For an overview of the codebase layout and data flow, see
[ARCHITECTURE.md](ARCHITECTURE.md).

---

## Adding a New Compressor

Every compressor in LossBench implements the `Compressor` interface
defined in `src/compressors/Compressor.hpp`. The benchmark driver never
touches concrete compressor types directly — it constructs them by name
through the factory in `src/compressors/factory.cpp`.

Adding a new compressor requires three files to change:

1. The compressor implementation (new `.hpp` / `.cpp` files)
2. `src/compressors/factory.cpp` (registration)
3. `src/compressors/CMakeLists.txt` (build integration)

### Step 1: Implement the `Compressor` Interface

Create `src/compressors/FooCompressor.hpp` and
`src/compressors/FooCompressor.cpp`. Your class must implement every
pure virtual method in the `Compressor` base class:

```cpp
// FooCompressor.hpp
#pragma once

#include <map>
#include <string>
#include <vector>

#include "Compressor.hpp"

class FooCompressor : public Compressor {
public:
    CompressedData compress(const std::vector<float>& data) override;
    std::vector<float> decompress(const CompressedData& compressedData) override;
    void configure(const std::map<std::string, std::string>& options) override;
    std::map<std::string, std::string> getConfig() const override;
    std::string name() const override;
    std::string description() const override;
    std::string version() const override;
    std::string usage() const override;

private:
    int _someOption = 42;
};
```

The methods break down as follows:

| Method | Purpose |
|---|---|
| `compress` | Takes a `vector<float>`, returns `CompressedData` (byte buffer + float count). |
| `decompress` | Takes a `CompressedData`, returns `vector<float>`. |
| `configure` | Parses a `key=value` option map from the CLI. Throw `std::invalid_argument` on unknown keys. |
| `getConfig` | Returns the current config as a string map — serialized into each JSONL result row. |
| `name` | Short CLI identifier (e.g. `"foo"`). This is the string users pass to `--compressor`. |
| `description` | One-line description shown by `--list-compressors`. |
| `version` | Library version string, or `""` if not applicable. |
| `usage` | Multi-line help text shown by `--compressor-help foo`. |

There is also one optional virtual method:

| Method | Purpose |
|---|---|
| `setDimensions` | Called once per chunk with the logical shape (e.g. `{nRows, rowLen}` for 2-D layouts). The default is a no-op. Override this if your compressor supports multi-dimensional input natively. |

**Implementation notes:**

- `compress` must set `CompressedData::numFloats` to `data.size()` so
  that `decompress` can pre-size its output buffer.
- `configure` is called once before any compress/decompress calls. It
  should throw on unrecognized option keys so typos fail loudly.
- `compress` and `decompress` should not retain state between calls
  beyond the configured options.

See `src/compressors/ZstdCompressor.cpp` for a minimal example wrapping
an external library, or `src/compressors/UniformHistogramCompressor.cpp`
for a self-contained compressor with no external dependency.

### Step 2: Register in the Factory

Open `src/compressors/factory.cpp` and add your compressor.

If your compressor has **no external dependency** (or uses one that is
always available, like zstd), add it unconditionally:

```cpp
#include "FooCompressor.hpp"

// In the factories map:
{"foo", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<FooCompressor>(); }},
```

If your compressor wraps an **optional library**, gate it behind a
`HAS_FOO` macro:

```cpp
#ifdef HAS_FOO
#include "FooCompressor.hpp"
#endif

// In the factories map:
#ifdef HAS_FOO
    {"foo", +[]() -> std::unique_ptr<Compressor> { return std::make_unique<FooCompressor>(); }},
#endif
```

### Step 3: Update CMakeLists.txt

Open `src/compressors/CMakeLists.txt`.

**If the compressor has no external dependency**, add the source files
to the `COMPRESSOR_SOURCES` list:

```cmake
set(COMPRESSOR_SOURCES
    # ... existing sources ...
    FooCompressor.cpp
    FooCompressor.hpp
)
```

**If the compressor wraps an optional library**, follow the existing
pattern — find the dependency, conditionally append sources, link the
library, and export a `HAS_FOO` compile definition:

```cmake
# Find the dependency
find_package(Foo QUIET)

# Conditionally add sources and link
if(Foo_FOUND)
    message(STATUS "Foo found - enabling Foo compressor")
    list(APPEND COMPRESSOR_SOURCES FooCompressor.cpp FooCompressor.hpp)
    list(APPEND COMPRESSOR_LIBS Foo::Foo)
    set(HAS_FOO TRUE)
else()
    message(STATUS "Foo not found - Foo compressor disabled")
    set(HAS_FOO FALSE)
endif()

# ... later, after add_library(compressors ...) ...

# Export the compile definition
if(HAS_FOO)
    target_compile_definitions(compressors PUBLIC HAS_FOO)
endif()

# Propagate to parent scope for tests
set(HAS_FOO ${HAS_FOO} PARENT_SCOPE)
```

If the library doesn't ship a CMake config, use `find_library` /
`find_path` instead of `find_package` — see the SPERR or ISABELA blocks
in `CMakeLists.txt` for examples.

### Verify

Rebuild and confirm your compressor appears:

```bash
cmake --build build -j$(nproc)
./build/lossbench --list-compressors      # should include "foo"
./build/lossbench --compressor-help foo   # should print your usage() text
```

---

## Adding a New Metric

Metrics in LossBench use a self-registering pattern: each metric file
defines a lambda that writes key-value pairs into the JSON result
object, and registers it at static-init time. No central list needs to
be updated — just write the file and add it to the build.

The existing metric files are:

- `src/benchmark/throughput_metrics.cpp` — compression ratio, throughput
- `src/benchmark/error_metrics.cpp` — point-wise error (abs, rel, MSE, PSNR)
- `src/benchmark/distribution_metrics.cpp` — KS test, Wasserstein, quantile shifts

### Step 1: Write the Metric File

Create a new file in `src/benchmark/`, e.g.
`src/benchmark/foo_metrics.cpp`:

```cpp
#include "benchmark.hpp"

static auto reg = [] {
    registerMetric([](const MetricContext& ctx, nlohmann::json& out) {
        // Compute your metric from ctx, write results into out.
        // Example: fraction of values that changed
        std::size_t n = ctx.original.size();
        if (n == 0) return;

        std::size_t changed = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (ctx.original[i] != ctx.decompressed[i]) ++changed;
        }
        out["fraction_changed"] = static_cast<double>(changed) / n;
    });
    return 0;
}();
```

The `static auto reg = [] { ... return 0; }();` pattern ensures your
metric is registered before `main` runs. Multiple metrics can be
registered in the same file if they are logically related.

### Available Inputs

The `MetricContext` struct (defined in `src/benchmark/benchmark.hpp`)
gives you access to:

| Field | Type | Description |
|---|---|---|
| `original` | `const vector<float>&` | Original input data |
| `decompressed` | `const vector<float>&` | Reconstructed data after compress/decompress |
| `sortedOriginal` | `const vector<float>&` | Ascending-sorted copy of `original` |
| `sortedDecompressed` | `const vector<float>&` | Ascending-sorted copy of `decompressed` |
| `originalSizeBytes` | `size_t` | Total bytes in the original input |
| `compressedSizeBytes` | `size_t` | Total bytes produced by the compressor |
| `compressionTimeMs` | `double` | Wall-clock compression time in milliseconds |
| `decompressionTimeMs` | `double` | Wall-clock decompression time in milliseconds |

The pre-sorted copies are provided so that metrics needing order
statistics (quantiles, KS distance, etc.) don't each pay for their own
sort.

### Step 2: Add to CMakeLists.txt

Add your new file to the object library in
`src/benchmark/CMakeLists.txt`:

```cmake
add_library(benchmark OBJECT
    benchmark.hpp
    benchmark.cpp
    throughput_metrics.cpp
    error_metrics.cpp
    distribution_metrics.cpp
    foo_metrics.cpp            # <-- add here
)
```

That's it. The self-registration pattern means no other file needs to
change.

### Verify

Rebuild and run a benchmark — your new keys should appear in the JSONL
output:

```bash
cmake --build build -j$(nproc)
./build/lossbench \
    --inputFile data.root \
    --tree MyTree \
    --branches branch1 \
    --chunkSize 0 \
    --compressor zstd \
    --resultsFile /dev/stdout | python3 -m json.tool
```

Look for your new keys (e.g. `"fraction_changed"`) in the `results`
object.

### Tips

- Keep metric keys descriptive and snake_case to match existing
  conventions.
- If your metric is expensive, consider whether it can share
  pre-computed data with existing metrics (the sorted vectors are
  already shared this way).
- If your metric needs data that isn't in `MetricContext`, you'll need
  to add a field there (`src/benchmark/benchmark.hpp`) and populate it
  in `runChunkedBenchmark` (`src/benchmark/benchmark.cpp`).
