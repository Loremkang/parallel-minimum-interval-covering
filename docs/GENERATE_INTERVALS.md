# generate_intervals Implementation

## Overview

The `test_utils::generate_intervals()` function provides a unified, parallel implementation for generating test intervals that satisfy all required constraints for the `IntervalCovering` algorithm.

## Location

[`include/test_utils.h`](../include/test_utils.h)

## Constraints Guaranteed

The generated intervals satisfy these properties (verified in [`interval_covering.h:405-420`](../include/interval_covering.h)):

1. **Strict Monotonicity**: `L(i) < L(i+1) && R(i) < R(i+1)`
   - Left and right endpoints are strictly increasing

2. **Valid Intervals**: `L(i) < R(i)`
   - Every interval has positive length

3. **No Gaps**: `L(i+1) <= R(i)` **MUST hold for all i**
   - Consecutive intervals must touch or overlap (no gaps allowed)

## Implementation

Uses parallel construction via ParlayLib primitives:

```cpp
// Generate random numbers sequentially (ensures reproducibility)
std::mt19937 rng(seed);
parlay::sequence<int> steps(n);
parlay::sequence<int> lens(n);
for (size_t i = 0; i < n; i++) {
  steps[i] = step_dist(rng);
  lens[i] = len_dist(rng);
}

// Compute left endpoints via parallel prefix sum
parlay::sequence<int> lefts = steps;
parlay::scan_inplace(lefts);

// Compute right endpoints in parallel
parlay::sequence<int> rights = parlay::tabulate(n, [&](size_t i) {
  return lefts[i] + lens[i];
});
```

### Why This Works

- **L(i) monotonicity**: `scan_inplace` computes prefix sums, guaranteeing `L(i) < L(i+1)`
- **R(i) strict monotonicity**: Since `R(i+1) = L(i) + step + len(i+1)` and `R(i) = L(i) + len(i)`, we need `len(i) < step + len(i+1)`
  - Worst case: `len_max - len_min < step_min` → `24 - 20 = 4 < 5` ✓
  - This **guarantees** `R(i) < R(i+1)` for all i
- **Valid intervals**: `len_min = 20` ensures `R(i) > L(i)` ✓
- **No gaps (CRITICAL)**: Since `L(i+1) = L(i) + step` and `R(i) = L(i) + len`, and `step <= len` (guaranteed by `step_max <= len_min`):
  - `L(i+1) = L(i) + step <= L(i) + len = R(i)` (since `step_max = 15 <= len_min = 20`)
  - This **guarantees** `L(i+1) <= R(i)` for all i, ensuring no gaps

## Parameters

```cpp
parlay::sequence<std::pair<int, int>> generate_intervals(
    size_t n,              // Number of intervals
    int seed = 42,         // Random seed for reproducibility
    int step_min = 5,      // Min step between left endpoints
    int step_max = 15,     // Max step between left endpoints
    int len_min = 20,      // Min interval length
    int len_max = 24       // Max interval length
);
```

**CRITICAL CONSTRAINTS (must hold for correctness):**
1. `step_max <= len_min` → guarantees no gaps: `L(i+1) <= R(i)`
2. `step_min > len_max - len_min` → guarantees `R(i) < R(i+1)`

### Default Parameters

The defaults are chosen to create interesting test cases while **guaranteeing all constraints**:
- `step_dist(5, 15)`: Allows various spacing between intervals
- `len_dist(20, 24)`: Creates intervals of varied lengths
- **Constraint 1:** `step_max (15) <= len_min (20)` ensures no gaps ✓
- **Constraint 2:** `step_min (5) > len_max - len_min (4)` ensures R(i) strictly increasing ✓
- Produces ~100% overlapping intervals with default parameters

## Usage

### Basic Usage

```cpp
#include "test_utils.h"

auto intervals = test_utils::generate_intervals(10000);
auto getL = [&](size_t i) { return intervals[i].first; };
auto getR = [&](size_t i) { return intervals[i].second; };

IntervalCovering solver(intervals.size(), getL, getR);
solver.Run();
```

### Custom Parameters

```cpp
// Generate intervals with more variation (but still satisfying all constraints)
auto intervals = test_utils::generate_intervals(
    10000,  // n
    42,     // seed
    10,     // step_min
    25,     // step_max
    25,     // len_min (MUST be >= step_max = 25!)
    34      // len_max (len_max - len_min = 9 < step_min = 10 ✓)
);
```

### Reproducibility

```cpp
// Same seed produces identical intervals
auto intervals1 = test_utils::generate_intervals(1000, 42);
auto intervals2 = test_utils::generate_intervals(1000, 42);
// intervals1 == intervals2

// Different seeds produce different intervals
auto intervals3 = test_utils::generate_intervals(1000, 123);
// intervals3 != intervals1
```

## Testing

Comprehensive tests in [`tests/test_generate_intervals.cpp`](../tests/test_generate_intervals.cpp) verify:
- All three constraints are satisfied
- Seed variation and reproducibility
- Custom parameters work correctly
- IntervalCovering compatibility
- Statistics on overlapping vs touching intervals

Run the test:
```bash
cd build
./bin/test_generate_intervals
```

## Migration

All benchmark and debug scripts now use this unified implementation:
- [`tests/debug_comparison.cpp`](../tests/debug_comparison.cpp)
- [`tests/test_simple_scan.cpp`](../tests/test_simple_scan.cpp)
- [`benchmarks/benchmark_interval_covering.cpp`](../benchmarks/benchmark_interval_covering.cpp)
- [`benchmarks/benchmark_parallel_breakdown.cpp`](../benchmarks/benchmark_parallel_breakdown.cpp)
- [`benchmarks/benchmark_thread_scaling.cpp`](../benchmarks/benchmark_thread_scaling.cpp)

This ensures consistent behavior across all test and benchmark code.

## Design Rationale

### Why Sequential Random Generation?

The random number generation is done sequentially (not in parallel) to ensure **reproducibility**:

```cpp
// Sequential generation (reproducible)
for (size_t i = 0; i < n; i++) {
  steps[i] = step_dist(rng);
  lens[i] = len_dist(rng);
}
```

If we used parallel generation with shared RNG, we would have:
- **Race conditions**: Multiple threads accessing the same `rng` object
- **Non-determinism**: Same seed could produce different results on different runs
- **Hard-to-debug tests**: Flaky test failures

By generating sequentially and then processing in parallel, we get:
- ✓ **Reproducibility**: Same seed always produces identical intervals
- ✓ **Thread safety**: No race conditions
- ✓ **Parallelism where it matters**: Prefix sum and interval construction still parallel
- ✓ **Negligible overhead**: Random generation is O(n), prefix sum is also O(n)

### Parameter Constraints

The constraints on parameters are mathematically derived to guarantee correctness:

**For no gaps (`L(i+1) <= R(i)`):**
```
L(i+1) = L(i) + step
R(i) = L(i) + len

Require: L(i) + step <= L(i) + len
Therefore: step <= len
Constraint: step_max <= len_min
```

**For strictly increasing R(i) (`R(i) < R(i+1)`):**
```
R(i) = L(i) + len(i)
R(i+1) = L(i+1) + len(i+1) = L(i) + step + len(i+1)

Require: L(i) + len(i) < L(i) + step + len(i+1)
Simplify: len(i) < step + len(i+1)
Rearrange: len(i) - len(i+1) < step

Worst case: len_max - len_min < step_min
Constraint: step_min > len_max - len_min
```

These constraints are enforced by assertions in the code:
```cpp
assert(step_max <= len_min && "step_max must be <= len_min to prevent gaps");
assert(step_min > len_max - len_min && "step_min must be > len_max - len_min to guarantee R(i) strictly increasing");
```
