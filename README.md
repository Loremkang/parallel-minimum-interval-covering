# Parallel Minimum Interval Cover

A high-performance parallel implementation of the Minimum Interval Cover problem using the ParlayLib parallel primitives library.

## Problem Description

The Minimum Interval Cover problem finds the minimum set of intervals needed to cover the union of all intervals. Given a set of intervals with monotonically increasing left and right endpoints, the algorithm efficiently identifies which intervals are necessary in the optimal covering.

## Algorithm Overview

The parallel algorithm uses an efficient sampling-based approach with five main phases:

1. **Find Furthest Intersecting Intervals**: For each interval, find the furthest (rightmost) interval that intersects with it. This uses parallel binary search and exploits the monotonicity property of the input.

2. **Sample Intervals**: Randomly sample a subset of intervals at a fixed rate to create a sparse sketch of the problem.

3. **Build Connections Between Samples**: For each sampled interval, determine which sampled interval it connects to by following the furthest-interval chain.

4. **Scan Sampled Intervals**: Perform a sequential scan over the sampled intervals to identify which are valid in the minimum cover.

5. **Scan Non-Sampled Intervals**: In parallel, scan the non-sampled intervals between each pair of valid sampled intervals to complete the solution.

This sampling-based approach is simple, efficient, and achieves good parallel scalability.

> **Note**: An alternative Euler tour based implementation is available in `interval_covering_euler.h`, but experiments show it is slower than the sampling-based approach.

## Features

- **Parallel and Serial Implementations**: Both parallel and serial versions for comparison and validation
- **Debug Mode**: Extensive validation by comparing parallel and serial results
- **Performance Optimizations**: Uses ParlayLib's efficient parallel primitives
- **Comprehensive Testing**: Unit tests covering various edge cases
- **Performance Benchmarking**: Tools to measure single-threaded and multi-threaded performance

## Requirements

- C++17 or later
- CMake 3.10 or later
- Ninja build system (recommended)
- Python 3.x with matplotlib and numpy (for plotting)
- [ParlayLib](https://github.com/cmuparlay/parlaylib) (included as submodule)
- Optional: numactl (for NUMA binding)

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

# Run tests
./bin/test_interval_covering

# Run benchmarks
./bin/benchmark_interval_covering
```

## Building with Debug Mode

To enable extensive validation:

```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DDEBUG=ON ..
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

// Create solver instance
IntervalCovering<decltype(getL), decltype(getR)> solver;
solver.n = intervals.size();
solver.L = getL;
solver.R = getR;

// Run the algorithm
solver.Run();

// Access results
for (size_t i = 0; i < solver.n; i++) {
    if (solver.valid[i]) {
        std::cout << "Interval " << i << " is in minimum cover\n";
    }
}
```

## Automated Benchmarking

The project includes automated benchmark scripts with centralized configuration for easy performance testing.

### Quick Start

Run all benchmarks with a single command:

```bash
./tools/run_all_benchmarks.sh
```

Or run individual benchmarks:

```bash
./tools/bench_thread_scaling.sh      # Thread scaling analysis
./tools/bench_parallel_breakdown.sh  # Parallel algorithm breakdown
```

### Benchmark Configuration

All benchmark parameters are centralized in `tools/benchmark_config.sh`:

```bash
# NUMA configuration
export NUMA_NODE=0

# Thread counts to test
export THREAD_COUNTS="1 2 4 8 12 16 20"

# Benchmark problem sizes (number of intervals)
export BENCHMARK_SIZES="10000 100000 1000000 10000000"

# Python virtual environment path
export VENV_PATH="venv"
```

### Benchmark Scripts

The benchmark infrastructure includes:

- **`bench_thread_scaling.sh`**: Measures performance across different thread counts
  - Automatically recompiles the project
  - Runs benchmarks with configured thread counts
  - Generates visualization plots (time, speedup, throughput, efficiency)

- **`bench_parallel_breakdown.sh`**: Analyzes time breakdown of parallel algorithm phases
  - Automatically recompiles the project
  - Measures time spent in each of the 5 algorithm phases:
    - BuildFurthest (parallel binary search)
    - SampleIntervals (random sampling)
    - BuildConnections (connection graph)
    - ScanSamples (sequential scan)
    - ScanNonsample (parallel scan)
  - Generates comprehensive visualization plots (stacked bar, scaling, percentage, speedup)

- **`run_all_benchmarks.sh`**: Runs all benchmarks sequentially

### Features

- **Automatic Recompilation**: Benchmark scripts rebuild the project before running
- **Python Virtual Environment**: Automatically activates venv for plotting scripts
- **NUMA Binding**: Optional NUMA node binding for consistent performance
- **Configurable Sizes**: Easily adjust test sizes without modifying code
- **CSV Output**: Results saved in `results/` directory
- **Automatic Plotting**: Generates PNG and PDF plots in `plots/` directory

### Manual Benchmark Execution

You can also run benchmarks manually with custom parameters:

```bash
# Thread scaling with custom sizes
cd results
PARLAY_NUM_THREADS=8 ../build/bin/benchmark_thread_scaling 1000 10000 100000

# Parallel breakdown with custom sizes
PARLAY_NUM_THREADS=16 ../build/bin/benchmark_parallel_breakdown 5000 50000
```

## Performance

The algorithm achieves significant speedup on multi-core systems. Run the automated benchmarks to see detailed performance analysis across different input sizes and thread counts.

## Project Structure

```
.
├── include/              # Header files
│   ├── interval_covering.h        # Main implementation (sampling-based)
│   ├── interval_covering_euler.h  # Legacy Euler tour implementation (reference only)
│   └── test_utils.h
├── tests/               # Test executables
├── benchmarks/          # Benchmark programs
│   ├── benchmark_thread_scaling.cpp
│   └── benchmark_parallel_breakdown.cpp
├── tools/               # Automated benchmark scripts
│   ├── benchmark_config.sh          # Centralized configuration
│   ├── bench_thread_scaling.sh      # Thread scaling benchmark
│   ├── bench_parallel_breakdown.sh  # Algorithm breakdown benchmark
│   ├── run_all_benchmarks.sh        # Run all benchmarks
│   ├── run_thread_scaling.sh        # Internal runner script
│   ├── run_parallel_breakdown.sh    # Internal runner script
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
