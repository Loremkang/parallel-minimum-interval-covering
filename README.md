# Parallel Minimum Interval Cover

A high-performance parallel implementation of the Minimum Interval Cover problem using the ParlayLib parallel primitives library.

## Problem Description

The Minimum Interval Cover problem finds the minimum set of intervals needed to cover the union of all intervals. Given a set of intervals with monotonically non-decreasing left and right endpoints, the algorithm efficiently identifies which intervals are necessary in the optimal covering.

## Algorithm Overview

The parallel algorithm uses an efficient sampling-based approach with five main phases:

1. **Find Furthest Intersecting Intervals**: For each interval, find the furthest (rightmost) interval that intersects with it. This uses parallel binary search and exploits the monotonicity property of the input.

2. **Sample Intervals**: Randomly sample a subset of intervals at a fixed rate to create a sparse sketch of the problem.

3. **Build Connections Between Samples**: For each sampled interval, determine which sampled interval it connects to by following the furthest-interval chain.

4. **Scan Sampled Intervals**: Perform a sequential scan over the sampled intervals to identify which are valid in the minimum cover.

5. **Scan Non-Sampled Intervals**: In parallel, scan the non-sampled intervals between each pair of valid sampled intervals to complete the solution.

This sampling-based approach is simple, efficient, and achieves good parallel scalability.

> **Note**: `Implementation::FineTuned` is the default policy and currently
> falls back to Sampling. Its dispatch policy will be tuned on `genoa3`.

## Features

- **Unified Implementation Selection**: FineTuned, Serial, Sampling, and EulerTour behind one public API
- **Opt-in Input Validation**: Explicit validation independent of build mode
- **Independent Cross-checks**: Tests compare all implementations and the default policy
- **Performance Optimizations**: Uses ParlayLib's efficient parallel primitives
- **Comprehensive Testing**: Unit tests covering various edge cases
- **Performance Benchmarking**: Tools to measure single-threaded and multi-threaded performance

## Requirements

- C++17 or later
- CMake 3.10 or later
- Ninja build system (recommended)
- Python 3.x with matplotlib and numpy (for plotting)
- [ParlayLib](https://github.com/cmuparlay/parlaylib) (included as submodule)
- numactl (required for canonical benchmarks on `genoa3`)

## Documentation

- [Algorithm and implementations](docs/ALGORITHM.md)
- [Reproducible benchmarking protocol](docs/BENCHMARKING.md)

## Building

```bash
# Clone the repository
git clone <repository-url>
cd Parallel-Minimum-Interval-Cover

# Initialize submodules
git submodule update --init --recursive

# Create Python virtual environment for plotting
python3 -m venv venv
source venv/bin/activate
pip install matplotlib numpy

# Build with Ninja (recommended)
mkdir build && cd build
cmake -G Ninja ..
ninja

# Or build with Make
cmake ..
make

# Run the core algorithm and input-validation tests
cmake --build . --target run_tests

# Run benchmarks
./bin/benchmark_thread_scaling
```

## Building with Debug Information

To enable debug symbols and internal assertions:

```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
ninja
```

## Usage Example

```cpp
#include "interval_covering.h"

// Define your interval data structure
std::vector<std::pair<int, int>> intervals = {
    {0, 5}, {1, 8}, {3, 10}, {7, 15}, {12, 20}
};

// Create lambda functions for accessing L and R
auto getL = [&](size_t i) { return intervals[i].first; };
auto getR = [&](size_t i) { return intervals[i].second; };

// Run the default FineTuned policy (currently Sampling)
auto selected = interval_covering::minimum_interval_cover(
    intervals.size(), getL, getR);

// Opt into an O(n) input validation pass:
auto checked = interval_covering::minimum_interval_cover<
    interval_covering::Implementation::Sampling,
    interval_covering::InputValidation::Enabled>(
    intervals.size(), getL, getR);

// Select a concrete implementation when needed:
auto selected_serial = interval_covering::minimum_interval_cover<
    interval_covering::Implementation::Serial>(
    intervals.size(), getL, getR);

auto selected_euler = interval_covering::minimum_interval_cover<
    interval_covering::Implementation::EulerTour>(
    intervals.size(), getL, getR);

// Access results
for (size_t i = 0; i < intervals.size(); i++) {
    if (selected[i]) {
        std::cout << "Interval " << i << " is in minimum cover\n";
    }
}
```

## Automated Benchmarking

The project includes automated benchmark scripts with centralized configuration for easy performance testing.

### Quick Start

Build, run, and plot both benchmark suites with one command:

```bash
./tools/run_benchmarks.sh all
```

Or run individual benchmarks:

```bash
./tools/run_benchmarks.sh scaling   # FineTuned/Serial/Sampling/EulerTour
./tools/run_benchmarks.sh breakdown # Sampling phase breakdown
```

### Benchmark Configuration

All benchmark parameters are centralized in `tools/benchmark_config.sh`:

```bash
# NUMA configuration
NUMA_NODE=0

# Thread counts to test
THREAD_COUNTS="1 2 4 8 12 16 20"

# Instances use n:target_cover_size. The realized answer size is approximate.
BENCHMARK_INSTANCES="100000:100 100000:10000 1000000:1000 1000000:100000 10000000:10000 10000000:1000000"

# Python virtual environment path
VENV_PATH="venv"
```

These are defaults. Override them for one run without editing the file:

```bash
THREAD_COUNTS="1 2 4 6 8 10" RUN_ID="local_m4" \
  ./tools/run_benchmarks.sh scaling
```

### Benchmark Scripts

The benchmark infrastructure includes:

- **`run_benchmarks.sh scaling`**: Compares FineTuned, Serial, Sampling, and EulerTour
  - Automatically recompiles the project
  - Measures FineTuned, Sampling, and EulerTour with every configured thread count
  - Measures the thread-independent serial baseline once
  - Records both the target and realized cover size
  - Checks that all four implementations select the same number of intervals
  - Generates visualization plots (time, speedup, throughput, efficiency)

- **`run_benchmarks.sh breakdown`**: Analyzes Sampling phase timings
  - Automatically recompiles the project
  - Measures time spent in each of the 5 algorithm phases:
    - BuildFurthest (parallel binary search)
    - SampleIntervals (random sampling)
    - BuildConnections (connection graph)
    - ScanSamples (sequential scan)
    - ScanNonsample (parallel scan)
  - Generates comprehensive visualization plots (stacked bar, scaling, percentage, speedup)

- **`run_benchmarks.sh all`**: Builds once, then runs and plots both suites

### Features

- **Automatic Recompilation**: Benchmark scripts rebuild the project before running
- **Python Virtual Environment**: Uses `venv/bin/python3` when available
- **NUMA Binding**: CPU and memory binding to NUMA node 0 for canonical runs
- **Configurable Instances**: Adjust interval count and target cover size independently
- **Non-destructive Output**: Run IDs keep prior CSV and plot results intact
- **Automatic Plotting**: Generates PNG and PDF plots below `plots/<run-id>/`

### Manual Benchmark Execution

You can also run benchmarks manually with custom parameters:

```bash
# Compare all implementations using custom n:target_cover_size instances
cd results
PARLAY_NUM_THREADS=8 ../build/bin/benchmark_thread_scaling \
  1000000:1000 1000000:100000

# Sampling phase breakdown with custom instances
PARLAY_NUM_THREADS=16 ../build/bin/benchmark_parallel_breakdown \
  1000000:1000 1000000:100000
```

## Performance

Run the canonical benchmarks on `genoa3` to tune FineTuned and compare all
implementations across input sizes and thread counts.

## Project Structure

```
.
├── include/
│   ├── interval_covering.h        # Public API and implementation selector
│   └── interval_covering/internal/
│       ├── common.h               # Shared validation and preprocessing
│       ├── sampling.h             # Default sampling implementation
│       └── euler_tour.h           # Alternative Euler-tour implementation
├── tests/
│   ├── test_interval_covering.cpp  # Algorithm correctness tests
│   └── test_input_validation.cpp   # Validation reliability tests
├── benchmarks/          # Benchmark programs
│   ├── test_utils.h               # Parameterized random input generator
│   ├── test_generate_intervals.cpp # Generator correctness tests
│   ├── benchmark_thread_scaling.cpp # All implementations across threads
│   └── benchmark_parallel_breakdown.cpp
├── tools/               # Automated benchmark scripts
│   ├── benchmark_config.sh          # Centralized configuration
│   ├── run_benchmarks.sh            # Build/run/plot entry point
│   ├── plot_thread_scaling.py       # Plotting script
│   └── plot_parallel_breakdown.py   # Plotting script
├── results/             # Benchmark CSV results (auto-created)
├── plots/               # Generated plots (auto-created)
├── build/               # Build directory
├── venv/                # Python virtual environment
└── parlaylib/           # ParlayLib submodule
```

## License

MIT License

## References

- [ParlayLib](https://github.com/cmuparlay/parlaylib)
