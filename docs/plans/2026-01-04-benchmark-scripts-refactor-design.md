# Benchmark Scripts Refactor Design

**Date:** 2026-01-04
**Author:** Design discussion with user
**Status:** Approved

## Overview

Refactor benchmark testing scripts to:
1. Add NUMA binding support (default: node 0)
2. Reorganize scripts into three categories: run, plot, and combined
3. Standardize output to `results/` and `plots/` directories

## Goals

- **NUMA Support**: Bind CPU and memory to NUMA node 0 by default for consistent performance
- **Separation of Concerns**: Split running benchmarks from generating plots
- **Composability**: Create modular scripts that can be combined
- **Standard Paths**: Use consistent directory structure for results and plots

## Architecture

### Directory Structure

```
Parallel-Minimum-Interval-Cover/
├── results/               # Benchmark output (CSV files)
│   ├── parallel_breakdown.csv
│   └── thread_scaling.csv
├── plots/                 # Generated graphs (PNG/PDF)
│   ├── breakdown_*.{png,pdf}
│   └── scaling_*.{png,pdf}
├── tools/                 # Testing scripts
│   ├── run_parallel_breakdown.sh       # Run benchmark → results/
│   ├── plot_parallel_breakdown.py      # Generate plots
│   ├── bench_parallel_breakdown.sh     # Run + plot
│   ├── run_thread_scaling.sh           # Run benchmark → results/
│   ├── plot_thread_scaling.py          # Generate plots
│   ├── bench_thread_scaling.sh         # Run + plot
│   └── run_all_benchmarks.sh           # Run all benchmarks
```

### Script Types

| Type | Extension | Purpose | Example |
|------|-----------|---------|---------|
| Run | `.sh` | Execute benchmark with numactl, output to `results/` | `run_parallel_breakdown.sh` |
| Plot | `.py` | Read from `results/`, generate graphs to `plots/` | `plot_parallel_breakdown.py` |
| Bench | `.sh` | Run + Plot (calls both above) | `bench_parallel_breakdown.sh` |
| All | `.sh` | Run all benchmarks | `run_all_benchmarks.sh` |

## Design Details

### 1. Run Scripts (`run_*.sh`)

**Template:**
```bash
#!/bin/bash
set -e  # Exit on error
set -u  # Error on undefined variables

# Configuration
NUMA_NODE=0  # Hardcoded NUMA configuration

# Path setup (relative to project root)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
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
BENCHMARK="$BUILD_DIR/benchmark_xxx"
if [ ! -f "$BENCHMARK" ]; then
    echo "Error: Benchmark not found at $BENCHMARK"
    echo "Please build first: cd build && cmake .. && ninja"
    exit 1
fi

# Run benchmark
OUTPUT_FILE="$RESULTS_DIR/xxx.csv"
echo "Running benchmark with NUMA node $NUMA_NODE..."
echo "Output: $OUTPUT_FILE"
$NUMACTL_CMD $BENCHMARK > "$OUTPUT_FILE"
echo "Done!"
```

**Key Features:**
- `NUMA_NODE=0`: Hardcoded configuration variable (easy to modify)
- `set -e` and `set -u`: Strict error handling
- Automatic `results/` directory creation
- numactl availability check with fallback
- Benchmark existence check
- All paths relative to project root

### 2. Plot Scripts (`plot_*.py`)

**Modifications to existing scripts:**
```python
#!/usr/bin/env python3

from pathlib import Path
import sys

# Path setup
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
RESULTS_DIR = PROJECT_ROOT / 'results'
PLOTS_DIR = PROJECT_ROOT / 'plots'

# Validate input exists
input_file = RESULTS_DIR / 'parallel_breakdown.csv'
if not input_file.exists():
    print(f"Error: {input_file} not found")
    print("Please run: tools/run_parallel_breakdown.sh")
    sys.exit(1)

# Create plots directory
PLOTS_DIR.mkdir(exist_ok=True)

# Read data
with open(input_file, 'r') as f:
    # ... existing plotting logic ...
    pass

# Save plots to PLOTS_DIR
# plt.savefig(PLOTS_DIR / 'breakdown_stacked.png', ...)
```

**Changes:**
- Replace hardcoded paths with `RESULTS_DIR` and `PLOTS_DIR`
- Add input file validation
- Ensure `plots/` directory exists

### 3. Bench Scripts (`bench_*.sh`)

**Template:**
```bash
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================="
echo "Running Parallel Breakdown Benchmark"
echo "========================================="

# Step 1: Run benchmark
echo ""
echo "[1/2] Running benchmark..."
"$SCRIPT_DIR/run_parallel_breakdown.sh"

# Step 2: Generate plots
echo ""
echo "[2/2] Generating plots..."
python3 "$SCRIPT_DIR/plot_parallel_breakdown.py"

echo ""
echo "========================================="
echo "Complete!"
echo "  Results: results/parallel_breakdown.csv"
echo "  Plots:   plots/breakdown_*.{png,pdf}"
echo "========================================="
```

**Key Features:**
- Calls `run_*.sh` then `plot_*.py` in sequence
- Clear progress indication
- Summary of output locations

### 4. Run All Script (`run_all_benchmarks.sh`)

```bash
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================"
echo "Running ALL Benchmarks"
echo "========================================"
echo ""

"$SCRIPT_DIR/bench_parallel_breakdown.sh"
echo ""
echo "----------------------------------------"
echo ""
"$SCRIPT_DIR/bench_thread_scaling.sh"

echo ""
echo "========================================"
echo "All benchmarks complete!"
echo "  Results: results/"
echo "  Plots:   plots/"
echo "========================================"
```

## Data Flow

```
benchmark_xxx (C++ binary)
    ↓ (numactl --cpunodebind=0 --membind=0)
results/xxx.csv
    ↓ (Python reads)
plots/xxx_{type}.{png,pdf}
```

## Error Handling

1. **Missing numactl**: Warn and continue without NUMA binding
2. **Missing benchmark**: Error with build instructions
3. **Missing results file**: Error with run instructions
4. **Script errors**: Fail fast with `set -e`

## Implementation Checklist

- [ ] Create `results/` directory (auto-created by scripts)
- [ ] Create `plots/` directory (auto-created by scripts)
- [ ] Modify `tools/run_parallel_breakdown.sh`
  - [ ] Add NUMA_NODE variable
  - [ ] Add numactl support
  - [ ] Change output to `results/parallel_breakdown.csv`
  - [ ] Add error handling
- [ ] Modify `tools/run_thread_scaling.sh`
  - [ ] Add NUMA_NODE variable
  - [ ] Add numactl support
  - [ ] Change output to `results/thread_scaling.csv`
  - [ ] Add error handling
- [ ] Rename and modify `tools/plot_breakdown.py` → `tools/plot_parallel_breakdown.py`
  - [ ] Update paths to use `results/` and `plots/`
  - [ ] Add input validation
- [ ] Rename and modify `tools/plot_performance.py` → `tools/plot_thread_scaling.py`
  - [ ] Update paths to use `results/` and `plots/`
  - [ ] Add input validation
- [ ] Create `tools/bench_parallel_breakdown.sh`
- [ ] Create `tools/bench_thread_scaling.sh`
- [ ] Create `tools/run_all_benchmarks.sh`
- [ ] Test all scripts
- [ ] Update documentation/README if needed

## Benefits

1. **Reproducible Performance**: NUMA binding ensures consistent benchmark results
2. **Modular Design**: Run and plot separately or together as needed
3. **Clear Organization**: Results and plots in standard locations
4. **Easy to Use**: Single command to run all benchmarks
5. **Robust**: Comprehensive error checking and graceful degradation

## Future Enhancements

- Make NUMA_NODE configurable via environment variable
- Add support for multiple NUMA nodes comparison
- Add script to clean results/plots directories
- Add timestamp to result files for history tracking
