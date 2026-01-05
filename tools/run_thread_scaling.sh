#!/bin/bash
set -e  # Exit on error
set -u  # Error on undefined variables

# Path setup (relative to project root)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Load centralized configuration
source "$SCRIPT_DIR/benchmark_config.sh"

# Setup Python virtual environment
if [ -d "$PROJECT_ROOT/$VENV_PATH" ]; then
    echo "Activating Python virtual environment..."
    source "$PROJECT_ROOT/$VENV_PATH/bin/activate"
else
    echo "WARNING: Python venv not found at $PROJECT_ROOT/$VENV_PATH"
    echo "Continuing without venv..."
fi
BUILD_DIR="$PROJECT_ROOT/build/bin"
RESULTS_DIR="$PROJECT_ROOT/results"

# Create results directory
mkdir -p "$RESULTS_DIR"

# Check if numactl is available
if ! command -v numactl &> /dev/null; then
    echo "WARNING: numactl not found, running without NUMA binding"
    NUMACTL_CMD=""
else
    NUMACTL_CMD="numactl --cpunodebind=$NUMA_NODE --membind=$NUMA_NODE"
fi

# Check if benchmark exists
BENCHMARK="$BUILD_DIR/benchmark_thread_scaling"
if [ ! -f "$BENCHMARK" ]; then
    echo "Error: benchmark_thread_scaling not found at $BUILD_DIR"
    echo "Please run: cd build && cmake .. && make benchmark_thread_scaling"
    exit 1
fi

# Remove old results file
OUTPUT_FILE="$RESULTS_DIR/thread_scaling.csv"
rm -f "$OUTPUT_FILE"

# Get system thread count
SYSTEM_THREADS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 8)
echo "System has $SYSTEM_THREADS threads"
echo "NUMA binding: node $NUMA_NODE"
echo "Output: $OUTPUT_FILE"
echo ""

# Filter thread counts to not exceed system max
FILTERED_COUNTS=""
for T in $THREAD_COUNTS; do
    if [ $T -le $SYSTEM_THREADS ]; then
        FILTERED_COUNTS="$FILTERED_COUNTS $T"
    fi
done

echo "Testing thread counts:$FILTERED_COUNTS"
echo "======================================"
echo ""

# Run benchmark for each thread count (from results directory so CSV is written there)
# NOTE: The benchmark binary writes 'thread_scaling.csv' to the current working directory.
(
  cd "$RESULTS_DIR" || { echo "ERROR: Cannot cd to $RESULTS_DIR"; exit 1; }

  for THREADS in $FILTERED_COUNTS; do
      echo "Running with $THREADS threads..."
      echo "--------------------------------------"

      if ! PARLAY_NUM_THREADS=$THREADS $NUMACTL_CMD "$BENCHMARK" $BENCHMARK_SIZES; then
          echo "ERROR: Benchmark failed with $THREADS threads"
          exit 1
      fi
      echo ""
  done
) || {
    echo "ERROR: Benchmark execution failed"
    exit 1
}

# Verify output file was created
if [ ! -f "$OUTPUT_FILE" ]; then
    echo "ERROR: Expected output file not found: $OUTPUT_FILE"
    echo "The benchmark may have failed to produce output"
    exit 1
fi

echo "======================================"
echo "Benchmarking complete!"
echo "Results saved to $OUTPUT_FILE"
echo ""
