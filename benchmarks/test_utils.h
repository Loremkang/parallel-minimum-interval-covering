#ifndef INTERVAL_COVERING_BENCHMARKS_TEST_UTILS_H
#define INTERVAL_COVERING_BENCHMARKS_TEST_UTILS_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "parlay/primitives.h"
#include "parlay/random.h"
#include "parlay/sequence.h"

namespace test_utils {

using Interval = std::pair<size_t, size_t>;

// Generates a reproducible random instance whose minimum-cover size is close
// to target_cover_size. The target controls the mean forward reach; it does
// not prescribe answer positions or require an exact answer size.
inline parlay::sequence<Interval> generate_intervals(
    size_t n, size_t target_cover_size, uint64_t seed = 42) {
  if (n == 0) {
    if (target_cover_size != 0) {
      throw std::invalid_argument(
          "an empty instance must have target_cover_size == 0");
    }
    return {};
  }
  if (n == 1) {
    if (target_cover_size != 1) {
      throw std::invalid_argument(
          "a one-interval instance must have target_cover_size == 1");
    }
    return {{0, 1}};
  }
  if (target_cover_size < 2 || target_cover_size > n) {
    throw std::invalid_argument(
        "target_cover_size must be between 2 and n");
  }

  parlay::random random(seed);
  constexpr uint64_t kRandomResolution = uint64_t{1} << 20;

  // A chain of target_cover_size nodes would have this mean forward reach.
  const double target_reach =
      static_cast<double>(n - 1) / static_cast<double>(target_cover_size - 1);
  constexpr double kExtensionCandidatesPerReach = 12.0;

  const auto build_furthest = [&](double reach_scale) {
    // Only a sparse random subset proposes long extensions. This keeps the
    // expected number of competing proposals in a reach-sized window fixed,
    // rather than letting it grow with reach_scale and forcing the envelope
    // to concentrate near the upper bound.
    const double extension_probability =
        std::min(1.0, kExtensionCandidatesPerReach / reach_scale);
    auto result = parlay::tabulate(n, [&](size_t i) {
      if (i + 1 == n) return n - 1;

      const double event_unit = static_cast<double>(
          random.ith_rand(4 * i + 1) % kRandomResolution) /
          kRandomResolution;
      const bool proposes_extension =
          i == 0 || event_unit < extension_probability;

      double randomized_reach = 1.0;
      if (proposes_extension) {
        const double reach_unit = static_cast<double>(
            random.ith_rand(4 * i + 2) % kRandomResolution) /
            kRandomResolution;
        // The first interval starts the random envelope. Giving it at least
        // one nominal reach avoids an artificial uncovered prefix when the
        // requested cover contains only a few intervals.
        const double reach_factor =
            i == 0 ? 1.0 + 0.5 * reach_unit : 0.5 + reach_unit;
        randomized_reach = reach_scale * reach_factor;
      }
      const size_t reach = std::max(
          size_t{1},
          static_cast<size_t>(std::min(
              static_cast<double>(n), std::floor(randomized_reach + 0.5))));
      return reach >= n - i ? n - 1 : i + reach;
    });
    parlay::scan_inclusive_inplace(result, parlay::maxm<size_t>());
    return result;
  };

  const auto cover_size = [&](const parlay::sequence<size_t>& furthest) {
    size_t count = 1;
    size_t current = 0;
    while (current + 1 < n) {
      current = furthest[current];
      ++count;
    }
    return count;
  };

  // The random envelope can shift the realized cover size. Measure that bias
  // once on this instance and correct only the global reach scale. This does
  // not choose answer positions, and the final answer is still allowed to
  // differ from target_cover_size.
  auto furthest = build_furthest(target_reach);
  const size_t initial_cover_size = cover_size(furthest);
  const double corrected_reach =
      target_reach * static_cast<double>(initial_cover_size - 1) /
      static_cast<double>(target_cover_size - 1);
  furthest = build_furthest(corrected_reach);

  // Random coordinate gaps make the interval endpoints irregular while
  // preserving their order. scan_inplace is an exclusive prefix sum, so the
  // first left endpoint is zero.
  auto left = parlay::tabulate(n, [&](size_t i) {
    return size_t{1} + random.ith_rand(4 * i + 3) % 16;
  });
  parlay::scan_inplace(left);

  // Pick a random coordinate between left[j] and left[j + 1] for every
  // possible furthest index j. These ranges are ordered and disjoint, so the
  // resulting right endpoints remain non-decreasing and realize exactly the
  // generated furthest relation.
  auto right_for_furthest = parlay::tabulate(n, [&](size_t j) {
    if (j + 1 == n) {
      return left[j] + 1 + random.ith_rand(4 * j + 4) % 16;
    }
    const size_t coordinate_choices = left[j + 1] - left[j];
    return left[j] +
           random.ith_rand(4 * j + 4) % coordinate_choices;
  });

  // Since furthest[i] >= i + 1, all non-final intervals have positive length
  // and consecutive intervals overlap or touch.
  return parlay::tabulate(n, [&](size_t i) -> Interval {
    return {left[i], right_for_furthest[furthest[i]]};
  });
}

}  // namespace test_utils

#endif
