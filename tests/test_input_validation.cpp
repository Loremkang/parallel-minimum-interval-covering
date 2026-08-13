#include "interval_covering.h"
#include "test_utils.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using Coordinate = int64_t;
using Interval = std::pair<Coordinate, Coordinate>;

[[noreturn]] void fail(const std::string& message) {
  std::cerr << "FAILED: " << message << '\n';
  std::exit(1);
}

void require(bool condition, const std::string& message) {
  if (!condition) fail(message);
}

template <typename Intervals>
void validate(const Intervals& intervals) {
  const auto left = [&](size_t i) { return intervals[i].first; };
  const auto right = [&](size_t i) { return intervals[i].second; };
  interval_covering::internal::common::validate_input(
      intervals.size(), left, right);
}

template <typename Function>
void expect_invalid_call(Function&& function,
                         const std::string& expected_message) {
  try {
    function();
  } catch (const std::invalid_argument& error) {
    require(error.what() == expected_message,
            "expected validation message '" + expected_message +
                "', received '" + error.what() + "'");
    return;
  } catch (const std::exception& error) {
    fail("validation threw the wrong exception type: " +
         std::string(error.what()));
  }
  fail("invalid input was accepted; expected: " + expected_message);
}

template <typename Intervals>
void expect_invalid(const Intervals& intervals,
                    const std::string& expected_message) {
  expect_invalid_call([&] { validate(intervals); }, expected_message);
}

std::vector<Interval> touching_intervals(size_t n) {
  std::vector<Interval> intervals;
  intervals.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    const Coordinate left = static_cast<Coordinate>(10 * i);
    intervals.emplace_back(left, left + 10);
  }
  return intervals;
}

void test_empty_and_singleton_inputs() {
  size_t accessor_calls = 0;
  const auto inaccessible = [&](size_t) -> Coordinate {
    ++accessor_calls;
    throw std::logic_error("empty input accessor must not be called");
  };
  interval_covering::internal::common::validate_input(
      0, inaccessible, inaccessible);
  require(accessor_calls == 0, "empty validation called an accessor");

  validate(std::vector<Interval>{{0, 1}});
  expect_invalid(std::vector<Interval>{{0, 0}},
                 "each interval must have a positive length");
  std::cout << "✓ Empty and singleton inputs\n";
}

void test_generated_valid_inputs() {
  const std::vector<std::pair<size_t, size_t>> instances = {
      {0, 0},       {1, 1},       {2, 2},       {3, 2},
      {3, 3},       {17, 2},      {17, 8},      {17, 17},
      {1999, 100},  {2000, 100},  {2001, 100},  {10000, 1000},
  };
  const std::vector<uint64_t> seeds = {
      0, 1, 42, 123, std::numeric_limits<uint64_t>::max()};

  size_t checked = 0;
  for (const auto& [n, target] : instances) {
    for (uint64_t seed : seeds) {
      const auto intervals =
          test_utils::generate_intervals(n, target, seed);
      validate(intervals);
      ++checked;
    }
  }
  require(checked == instances.size() * seeds.size(),
          "not all generated valid instances were checked");
  std::cout << "✓ " << checked
            << " generated instances accepted without false positives\n";
}

void test_each_error_at_every_position() {
  constexpr size_t n = 64;
  constexpr const char* non_positive =
      "each interval must have a positive length";
  constexpr const char* decreasing_left =
      "left endpoints must be non-decreasing";
  constexpr const char* decreasing_right =
      "right endpoints must be non-decreasing";
  constexpr const char* gap =
      "consecutive intervals must overlap or touch";

  for (size_t position = 0; position < n; ++position) {
    auto intervals = touching_intervals(n);
    intervals[position].second = intervals[position].first;
    expect_invalid(intervals, non_positive);
  }

  for (size_t position = 0; position + 1 < n; ++position) {
    auto intervals = touching_intervals(n);
    intervals[position + 1].first = intervals[position].first - 1;
    expect_invalid(intervals, decreasing_left);
  }

  for (size_t position = 0; position + 1 < n; ++position) {
    auto intervals = touching_intervals(n);
    intervals[position + 1].second = intervals[position].second - 1;
    expect_invalid(intervals, decreasing_right);
  }

  for (size_t position = 0; position + 1 < n; ++position) {
    auto intervals = touching_intervals(n);
    intervals[position].second = intervals[position + 1].first - 1;
    expect_invalid(intervals, gap);
  }
  std::cout << "✓ All validation errors detected at every possible position\n";
}

void test_first_error_is_deterministic() {
  auto intervals = touching_intervals(100000);
  intervals[123].second = intervals[124].first - 1;  // First: gap.
  intervals[90001].first =
      intervals[90000].first - 1;  // Later: decreasing left.

  for (size_t repetition = 0; repetition < 32; ++repetition) {
    expect_invalid(intervals,
                   "consecutive intervals must overlap or touch");
  }
  std::cout << "✓ Parallel search reports the first error deterministically\n";
}

void test_public_api_integration() {
  constexpr size_t n = 10000;
  const auto intervals = test_utils::generate_intervals(n, 1000, 42);
  const auto left = [&](size_t i) { return intervals[i].first; };
  const auto right = [&](size_t i) { return intervals[i].second; };

  const auto fine_tuned = interval_covering::minimum_interval_cover<
      interval_covering::Implementation::FineTuned,
      interval_covering::InputValidation::Enabled>(n, left, right);
  const auto serial = interval_covering::minimum_interval_cover<
      interval_covering::Implementation::Serial,
      interval_covering::InputValidation::Enabled>(n, left, right);
  const auto sampling = interval_covering::minimum_interval_cover<
      interval_covering::Implementation::Sampling,
      interval_covering::InputValidation::Enabled>(n, left, right);
  const auto euler = interval_covering::minimum_interval_cover<
      interval_covering::Implementation::EulerTour,
      interval_covering::InputValidation::Enabled>(n, left, right);
  const auto fine_tuned_unchecked =
      interval_covering::minimum_interval_cover<
          interval_covering::Implementation::FineTuned,
          interval_covering::InputValidation::Disabled>(n, left, right);
  const auto serial_unchecked = interval_covering::minimum_interval_cover<
      interval_covering::Implementation::Serial,
      interval_covering::InputValidation::Disabled>(n, left, right);
  const auto sampling_unchecked = interval_covering::minimum_interval_cover<
      interval_covering::Implementation::Sampling,
      interval_covering::InputValidation::Disabled>(n, left, right);
  const auto euler_unchecked = interval_covering::minimum_interval_cover<
      interval_covering::Implementation::EulerTour,
      interval_covering::InputValidation::Disabled>(n, left, right);

  require(fine_tuned == serial,
          "validated FineTuned result disagrees with Serial");
  require(sampling == serial,
          "validated Sampling result disagrees with Serial");
  require(euler == serial,
          "validated EulerTour result disagrees with Serial");
  require(fine_tuned == fine_tuned_unchecked,
          "validation changed the FineTuned result");
  require(serial == serial_unchecked,
          "validation changed the Serial result");
  require(sampling == sampling_unchecked,
          "validation changed the Sampling result");
  require(euler == euler_unchecked,
          "validation changed the EulerTour result");

  const std::vector<Interval> invalid = {{0, 5}, {6, 10}};
  const auto invalid_left = [&](size_t i) { return invalid[i].first; };
  const auto invalid_right = [&](size_t i) { return invalid[i].second; };
  const auto expect_public_rejection = [&](auto implementation_tag) {
    constexpr auto implementation = decltype(implementation_tag)::value;
    expect_invalid_call(
        [&] {
          (void)interval_covering::minimum_interval_cover<
              implementation, interval_covering::InputValidation::Enabled>(
              invalid.size(), invalid_left, invalid_right);
        },
        "consecutive intervals must overlap or touch");
  };
  expect_public_rejection(std::integral_constant<
                          interval_covering::Implementation,
                          interval_covering::Implementation::FineTuned>{});
  expect_public_rejection(std::integral_constant<
                          interval_covering::Implementation,
                          interval_covering::Implementation::Serial>{});
  expect_public_rejection(std::integral_constant<
                          interval_covering::Implementation,
                          interval_covering::Implementation::Sampling>{});
  expect_public_rejection(std::integral_constant<
                          interval_covering::Implementation,
                          interval_covering::Implementation::EulerTour>{});
  std::cout << "✓ Validation preserves all implementation results\n";
}

}  // namespace

int main() {
#ifdef NDEBUG
  std::cout << "validate_input reliability tests (NDEBUG)\n";
#else
  std::cout << "validate_input reliability tests (Debug assertions enabled)\n";
#endif
  std::cout << "==========================================================\n";

  test_empty_and_singleton_inputs();
  test_generated_valid_inputs();
  test_each_error_at_every_position();
  test_first_error_is_deterministic();
  test_public_api_integration();

  std::cout << "==========================================================\n"
            << "All validate_input reliability tests passed\n";
}
