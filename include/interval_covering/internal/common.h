#ifndef INTERVAL_COVERING_INTERNAL_COMMON_H
#define INTERVAL_COVERING_INTERNAL_COMMON_H

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "parlay/parallel.h"
#include "parlay/primitives.h"

namespace interval_covering {
namespace internal {
namespace common {

inline constexpr size_t kParallelMergeSize = 2000;

template <typename GetL, typename GetR>
void validate_input(size_t n, const GetL& left, const GetR& right) {
  enum class ValidationError {
    None,
    NonPositiveLength,
    DecreasingLeftEndpoints,
    DecreasingRightEndpoints,
    Gap,
  };

  // Evaluate validation lazily so checking can run in parallel without
  // allocating an O(n) error array. No exception is thrown by a worker.
  auto errors = parlay::delayed_tabulate(n, [&](size_t i) {
    if (!(left(i) < right(i))) {
      return ValidationError::NonPositiveLength;
    }
    if (i + 1 == n) return ValidationError::None;
    if (left(i) > left(i + 1)) {
      return ValidationError::DecreasingLeftEndpoints;
    }
    if (right(i) > right(i + 1)) {
      return ValidationError::DecreasingRightEndpoints;
    }
    if (left(i + 1) > right(i)) {
      return ValidationError::Gap;
    }
    return ValidationError::None;
  });

  // find_if returns the first invalid position; throw afterwards on the
  // calling thread so error reporting is deterministic and exception-safe.
  const auto first_error = parlay::find_if(errors, [](ValidationError error) {
    return error != ValidationError::None;
  });
  if (first_error == errors.end()) return;

  switch (*first_error) {
    case ValidationError::NonPositiveLength:
      throw std::invalid_argument(
          "each interval must have a positive length");
    case ValidationError::DecreasingLeftEndpoints:
      throw std::invalid_argument(
          "left endpoints must be non-decreasing");
    case ValidationError::DecreasingRightEndpoints:
      throw std::invalid_argument(
          "right endpoints must be non-decreasing");
    case ValidationError::Gap:
      throw std::invalid_argument(
          "consecutive intervals must overlap or touch");
    case ValidationError::None:
      break;
  }
}

template <typename GetL, typename GetR>
void serial_minimum_interval_cover(
    size_t n, const GetL& left, const GetR& right,
    parlay::sequence<bool>& selected) {
  if (selected.size() != n) selected = parlay::sequence<bool>(n, false);
  if (n == 0) return;

  selected[0] = true;
  selected[n - 1] = true;
  size_t current = 0;
  for (size_t i = 1; i + 1 < n; ++i) {
    if (left(i + 1) > right(current)) {
      selected[i] = true;
      current = i;
    } else {
      selected[i] = false;
    }
  }
}

// Builds furthest[i] = max { j | left(j) <= right(i) }. Both endpoint
// sequences are non-decreasing, so the search ranges are monotone and can be
// split recursively without examining every pair of intervals.
template <typename GetL, typename GetR>
class FurthestIndexBuilder {
 public:
  using LeftEndpoint = std::remove_cv_t<std::remove_reference_t<
      decltype(std::declval<GetL>()(size_t{0}))>>;
  using RightEndpoint = std::remove_cv_t<std::remove_reference_t<
      decltype(std::declval<GetR>()(size_t{0}))>>;

  static_assert(std::is_same<LeftEndpoint, RightEndpoint>::value,
                "GetL and GetR must return the same type");

  FurthestIndexBuilder(size_t n, const GetL& left, const GetR& right)
      : n_(n), left_(left), right_(right), furthest_(n) {}

  parlay::sequence<size_t> Build() {
    if (n_ == 0) return {};

    // Recursively split the output range. Independent left and right halves
    // run in parallel and small subproblems use a cache-friendly linear scan.
    BuildParallelCore(0, n_ - 1, 0, n_ - 1);

    return std::move(furthest_);
  }

 private:
  void BuildSerial(size_t ll, size_t lr, size_t rl, size_t rr) {
    // right_id never moves backwards because both endpoint sequences are
    // non-decreasing.
    size_t right_id = rl;
    for (size_t i = ll; i <= lr; ++i) {
      const LeftEndpoint right_of_i = right_(i);
      while (right_id <= rr && left_(right_id) <= right_of_i) {
        ++right_id;
      }
      furthest_[i] = right_id - 1;
    }
  }

  void BuildParallelCore(size_t ll, size_t lr, size_t rl, size_t rr) {
    if (lr - ll + 1 + rr - rl + 1 <= kParallelMergeSize) {
      BuildSerial(ll, lr, rl, rr);
      return;
    }

    // Find the middle output with binary search. Its result partitions the
    // candidate range for the two recursive calls.
    const size_t left_mid = (ll + lr) >> 1;
    const LeftEndpoint right_of_mid = right_(left_mid);
    size_t left = std::max(left_mid, rl);
    size_t right = rr + 1;
    while (left + 1 < right) {
      const size_t mid = (left + right) >> 1;
      if (left_(mid) <= right_of_mid) {
        left = mid;
      } else {
        right = mid;
      }
    }
    furthest_[left_mid] = left;

    parlay::par_do(
        [&] {
          if (ll < left_mid) {
            BuildParallelCore(ll, left_mid - 1, rl, left);
          }
        },
        [&] {
          if (left_mid < lr) {
            BuildParallelCore(left_mid + 1, lr, left, rr);
          }
        });
  }

  size_t n_;
  const GetL& left_;
  const GetR& right_;
  parlay::sequence<size_t> furthest_;
};

template <typename GetL, typename GetR>
parlay::sequence<size_t> build_furthest(
    size_t n, const GetL& left, const GetR& right) {
  return FurthestIndexBuilder<GetL, GetR>(n, left, right).Build();
}

}  // namespace common
}  // namespace internal
}  // namespace interval_covering

#endif
