#include "interval_covering.h"
#include "test_utils.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

size_t serial_cover_size(const parlay::sequence<test_utils::Interval>& intervals) {
  const auto left = [&](size_t i) { return intervals[i].first; };
  const auto right = [&](size_t i) { return intervals[i].second; };
  const auto selected = interval_covering::minimum_interval_cover<
      interval_covering::Implementation::Serial>(
      intervals.size(), left, right);
  return static_cast<size_t>(
      std::count(selected.begin(), selected.end(), true));
}

void test_constraints() {
  constexpr size_t n = 10000;
  for (size_t target : {size_t{2}, size_t{10}, size_t{100}, size_t{1000}, n}) {
    const auto intervals = test_utils::generate_intervals(n, target);
    assert(intervals.size() == n);
    for (size_t i = 0; i < n; ++i) {
      assert(intervals[i].first < intervals[i].second);
      if (i + 1 == n) continue;
      assert(intervals[i].first < intervals[i + 1].first);
      assert(intervals[i].second <= intervals[i + 1].second);
      assert(intervals[i + 1].first <= intervals[i].second);
    }

    const auto left = [&](size_t i) { return intervals[i].first; };
    const auto right = [&](size_t i) { return intervals[i].second; };
    (void)interval_covering::minimum_interval_cover<
        interval_covering::Implementation::Sampling,
        interval_covering::InputValidation::Enabled>(n, left, right);
  }
  std::cout << "✓ Generated instances satisfy the input contract\n";
}

void test_seed_behavior() {
  constexpr size_t n = 1000;
  constexpr size_t target = 100;
  const auto first = test_utils::generate_intervals(n, target, 42);
  const auto repeated = test_utils::generate_intervals(n, target, 42);
  const auto other = test_utils::generate_intervals(n, target, 123);

  assert(first == repeated);
  assert(first != other);
  std::cout << "✓ Seeds are reproducible and vary the generated instance\n";
}

void test_target_controls_cover_size() {
  constexpr size_t n = 100000;
  const std::vector<size_t> targets = {10, 100, 1000, 10000};
  const std::vector<uint64_t> seeds = {0, 1, 42, 123, 999};
  std::vector<size_t> previous_actual(seeds.size(), 0);

  for (size_t target : targets) {
    size_t minimum_actual = n;
    size_t maximum_actual = 0;
    for (size_t seed_index = 0; seed_index < seeds.size(); ++seed_index) {
      const auto intervals = test_utils::generate_intervals(
          n, target, seeds[seed_index]);
      const size_t actual = serial_cover_size(intervals);

      // target_cover_size is approximate, but it should be a useful control
      // parameter rather than merely a label.
      assert(actual >= target * 3 / 4);
      assert(actual <= target * 4 / 3 + 1);
      assert(actual > previous_actual[seed_index]);
      previous_actual[seed_index] = actual;
      minimum_actual = std::min(minimum_actual, actual);
      maximum_actual = std::max(maximum_actual, actual);
    }

    std::cout << "  target=" << target << ", actual range="
              << minimum_actual << ".." << maximum_actual << '\n';
  }
  std::cout << "✓ Target cover size controls the realized answer scale\n";
}

void test_effective_reach_remains_broad() {
  constexpr size_t n = 100000;
  for (size_t target : {size_t{100}, size_t{1000}, size_t{10000}}) {
    const auto intervals = test_utils::generate_intervals(n, target, 42);
    const auto left = [&](size_t i) { return intervals[i].first; };
    const auto right = [&](size_t i) { return intervals[i].second; };
    const auto furthest = interval_covering::internal::common::build_furthest(
        n, left, right);

    const double target_reach =
        static_cast<double>(n - 1) / static_cast<double>(target - 1);
    const size_t interior_end =
        n - static_cast<size_t>(std::ceil(2.0 * target_reach));
    std::vector<size_t> reaches;
    reaches.reserve(interior_end);
    for (size_t i = 0; i < interior_end; ++i) {
      reaches.push_back(furthest[i] - i);
    }
    std::sort(reaches.begin(), reaches.end());

    const size_t p05 = reaches[reaches.size() * 5 / 100];
    const size_t p50 = reaches[reaches.size() * 50 / 100];
    const size_t p95 = reaches[reaches.size() * 95 / 100];

    // This guards against reverting to an extension proposal at every index.
    // In that construction, the prefix maximum concentrates almost all
    // effective reaches near the upper bound when target_reach is large.
    assert(static_cast<double>(p95 - p05) >= 0.35 * target_reach);
    std::cout << "  target=" << target
              << ", effective reach / target reach: p05="
              << p05 / target_reach << ", p50=" << p50 / target_reach
              << ", p95=" << p95 / target_reach << '\n';
  }
  std::cout << "✓ Effective reach distribution remains broad\n";
}

void test_implementations_agree() {
  constexpr size_t n = 10000;
  constexpr size_t target = 1000;
  const auto intervals = test_utils::generate_intervals(n, target);
  const auto left = [&](size_t i) { return intervals[i].first; };
  const auto right = [&](size_t i) { return intervals[i].second; };

  const auto sampling = interval_covering::minimum_interval_cover<
      interval_covering::Implementation::Sampling>(n, left, right);
  const auto euler = interval_covering::minimum_interval_cover<
      interval_covering::Implementation::EulerTour>(n, left, right);
  const auto fine_tuned = interval_covering::minimum_interval_cover<
      interval_covering::Implementation::FineTuned>(n, left, right);
  const auto default_implementation =
      interval_covering::minimum_interval_cover(n, left, right);
  const auto serial = interval_covering::minimum_interval_cover<
      interval_covering::Implementation::Serial>(n, left, right);
  assert(default_implementation == fine_tuned);
  assert(fine_tuned == sampling);
  assert(sampling == euler);
  assert(sampling == serial);
  std::cout << "✓ FineTuned, Serial, Sampling, and EulerTour agree\n";
}

void test_invalid_parameters() {
  const auto rejected = [](size_t n, size_t target) {
    try {
      (void)test_utils::generate_intervals(n, target);
      return false;
    } catch (const std::invalid_argument&) {
      return true;
    }
  };

  assert(test_utils::generate_intervals(0, 0).empty());
  assert(test_utils::generate_intervals(1, 1).size() == 1);
  assert(rejected(0, 1));
  assert(rejected(1, 0));
  assert(rejected(2, 1));
  assert(rejected(10, 11));
  std::cout << "✓ Invalid parameter combinations are rejected\n";
}

}  // namespace

int main() {
  std::cout << "Testing parameterized interval generation\n"
            << "========================================\n";
  test_constraints();
  test_seed_behavior();
  test_target_controls_cover_size();
  test_effective_reach_remains_broad();
  test_implementations_agree();
  test_invalid_parameters();
  std::cout << "========================================\n"
            << "All generator tests passed\n";
}
