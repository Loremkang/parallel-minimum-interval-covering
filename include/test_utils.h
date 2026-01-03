#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <cassert>
#include <random>
#include <utility>
#include "parlay/sequence.h"
#include "parlay/primitives.h"

namespace test_utils {

// Generate n intervals with guaranteed properties:
// - L(i) < L(i+1) && R(i) < R(i+1) (strict monotonicity)
// - L(i) < R(i) (valid intervals)
// - L(i+1) <= R(i) (no gaps between consecutive intervals)
//
// Uses parallel construction for efficiency.
//
// Parameters:
//   n: Number of intervals to generate
//   seed: Random seed for reproducibility (default: 42)
//   step_min: Minimum step between left endpoints (default: 5)
//   step_max: Maximum step between left endpoints (default: 15)
//   len_min: Minimum interval length (default: 20)
//   len_max: Maximum interval length (default: 24)
//
// CRITICAL CONSTRAINTS (must hold for correctness):
//   1. step_max <= len_min (guarantees no gaps: L(i+1) <= R(i))
//   2. step_min > len_max - len_min (guarantees R(i) < R(i+1))
inline parlay::sequence<std::pair<int, int>> generate_intervals(
    size_t n,
    int seed = 42,
    int step_min = 5,
    int step_max = 15,
    int len_min = 20,
    int len_max = 24) {

  // Validate parameters to guarantee all interval constraints
  assert(step_max <= len_min && "step_max must be <= len_min to prevent gaps");
  assert(step_min > len_max - len_min && "step_min must be > len_max - len_min to guarantee R(i) strictly increasing");

  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> step_dist(step_min, step_max);
  std::uniform_int_distribution<int> len_dist(len_min, len_max);

  // Generate random numbers sequentially to ensure reproducibility
  parlay::sequence<int> steps(n);
  parlay::sequence<int> lens(n);
  for (size_t i = 0; i < n; i++) {
    steps[i] = step_dist(rng);
    lens[i] = len_dist(rng);
  }

  // Compute left endpoints via prefix sum (parallel)
  parlay::sequence<int> lefts = steps;
  parlay::scan_inplace(lefts);

  // Generate right endpoints in parallel
  parlay::sequence<int> rights = parlay::tabulate(n, [&](size_t i) {
    return lefts[i] + lens[i];
  });

  // Combine into interval pairs (parallel)
  parlay::sequence<std::pair<int, int>> intervals = parlay::tabulate(n, [&](size_t i) {
    return std::make_pair(lefts[i], rights[i]);
  });

  return intervals;
}

} // namespace test_utils

#endif // TEST_UTILS_H
