#ifndef INTERVAL_COVERING_H
#define INTERVAL_COVERING_H

#include <cstddef>
#include <utility>

#include "interval_covering/internal/common.h"
#include "interval_covering/internal/euler_tour.h"
#include "interval_covering/internal/sampling.h"
#include "parlay/sequence.h"

namespace interval_covering {

enum class Implementation {
  FineTuned,
  Serial,
  Sampling,
  EulerTour,
};

enum class InputValidation {
  Disabled,
  Enabled,
};

// Returns a mask whose i-th element is true iff interval i is selected.
//
// The input must satisfy, for all applicable i:
//   left(i) <= left(i + 1)
//   right(i) <= right(i + 1)
//   left(i) < right(i)
//   left(i + 1) <= right(i)
// Set validation to InputValidation::Enabled to check these preconditions and
// throw std::invalid_argument on failure. Validation is disabled by default to
// avoid an additional O(n) pass.
template <Implementation implementation = Implementation::FineTuned,
          InputValidation validation = InputValidation::Disabled,
          typename GetL, typename GetR>
[[nodiscard]] parlay::sequence<bool> minimum_interval_cover(
    size_t n, GetL left, GetR right) {
  if constexpr (validation == InputValidation::Enabled) {
    internal::common::validate_input(n, left, right);
  } else {
    static_assert(validation == InputValidation::Disabled,
                  "unknown input-validation mode");
  }

  parlay::sequence<bool> selected;
  if constexpr (implementation == Implementation::FineTuned) {
    // FineTuned currently falls back to Sampling. Keep future machine- and
    // input-sensitive dispatch logic here so the policy remains visible at
    // the public API boundary.
    selected = internal::sampling::solve(
        n, std::move(left), std::move(right));
  } else if constexpr (implementation == Implementation::Serial) {
    internal::common::serial_minimum_interval_cover(
        n, left, right, selected);
  } else if constexpr (implementation == Implementation::Sampling) {
    selected = internal::sampling::solve(
        n, std::move(left), std::move(right));
  } else if constexpr (implementation == Implementation::EulerTour) {
    selected = internal::euler_tour::solve(
        n, std::move(left), std::move(right));
  } else {
    static_assert(implementation == Implementation::FineTuned,
                  "unknown interval-covering implementation");
  }

  return selected;
}

}  // namespace interval_covering

#endif
