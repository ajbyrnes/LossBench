#!/bin/bash
# Benchmark script for zlib compression with varying compression levels.

# Create a timestamped results directory to store output files:
#    results.jsonl -- results of running the benchmarks, in JSONL format
#    benchmark.log -- logging statements output by the benchmarking program, in plain text
# The timestamp uniquely identifies the run and prevents overwriting previous results
timestamp=$(date +"%Y%m%d_%H%M%S")

results_dir="results_sz3_$timestamp"
mkdir -p "$results_dir"

results_file="$results_dir/results.jsonl"
log_file="$results_dir/benchmark.log"

# Benchmark parameters
input_file="jets.root"
tree_name="CollectionTree"
branches="AnalysisJetsAuxDyn.pt,AnalysisJetsAuxDyn.eta,AnalysisJetsAuxDyn.phi,AnalysisJetsAuxDyn.m"
chunk_size=32768

# Run benchmark with sz3 for varying compression algorithms
for cmprAlgo in {0..4}
do
    echo "Running benchmark with sz3 compression algorithm $cmprAlgo..." >> "$log_file" 2>&1

    ./build/lossbench \
        --inputFile "$input_file" \
        --tree "$tree_name" \
        --branches "$branches" \
        --chunkSize "$chunk_size" \
        --compressor "sz3:cmprAlgo=$cmprAlgo" \
        --resultsFile "$results_file" >> "$log_file" 2>&1

    echo "Benchmark with sz3 compression algorithm $cmprAlgo completed." >> "$log_file" 2>&1
done
