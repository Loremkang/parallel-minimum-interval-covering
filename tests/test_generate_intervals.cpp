#include "test_utils.h"
#include "interval_covering.h"
#include <iostream>
#include <cassert>

// Test that generated intervals satisfy all required constraints
void test_interval_constraints() {
  const size_t n = 10000;
  auto intervals = test_utils::generate_intervals(n);

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  // Test constraint 1: L(i) < L(i+1) and R(i) < R(i+1)
  for (size_t i = 0; i < n - 1; i++) {
    assert(getL(i) < getL(i + 1));
    assert(getR(i) < getR(i + 1));
  }

  // Test constraint 2: L(i) < R(i)
  for (size_t i = 0; i < n; i++) {
    assert(getL(i) < getR(i));
  }

  // Test constraint 3: L(i+1) <= R(i) (no gaps - MUST hold for all i)
  size_t overlap_count = 0;
  size_t touching_count = 0;
  for (size_t i = 0; i < n - 1; i++) {
    assert(getL(i + 1) <= getR(i) && "Gap detected! L(i+1) > R(i)");
    if (getL(i + 1) < getR(i)) {
      overlap_count++;
    } else if (getL(i + 1) == getR(i)) {
      touching_count++;
    }
  }

  std::cout << "✓ All interval constraints satisfied\n";
  std::cout << "  - Overlapping: " << overlap_count << " / " << (n - 1) << "\n";
  std::cout << "  - Touching: " << touching_count << " / " << (n - 1) << "\n";
  std::cout << "  - Gaps: 0 (guaranteed)\n";
}

// Test that different seeds produce different intervals
void test_seed_variation() {
  const size_t n = 100;
  auto intervals1 = test_utils::generate_intervals(n, 42);
  auto intervals2 = test_utils::generate_intervals(n, 123);

  bool different = false;
  for (size_t i = 0; i < n; i++) {
    if (intervals1[i] != intervals2[i]) {
      different = true;
      break;
    }
  }

  assert(different);
  std::cout << "✓ Different seeds produce different intervals\n";
}

// Test that same seed produces same intervals (reproducibility)
void test_seed_reproducibility() {
  const size_t n = 100;
  auto intervals1 = test_utils::generate_intervals(n, 42);
  auto intervals2 = test_utils::generate_intervals(n, 42);

  for (size_t i = 0; i < n; i++) {
    assert(intervals1[i] == intervals2[i]);
  }

  std::cout << "✓ Same seed produces identical intervals (reproducible)\n";
}

// Test custom parameters
void test_custom_parameters() {
  const size_t n = 1000;
  // Test with different valid parameters
  auto intervals = test_utils::generate_intervals(n, 42, 10, 25, 25, 34);

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  // Verify constraints still hold
  for (size_t i = 0; i < n - 1; i++) {
    assert(getL(i) < getL(i + 1));
    assert(getR(i) < getR(i + 1));
  }

  for (size_t i = 0; i < n; i++) {
    assert(getL(i) < getR(i));
  }

  std::cout << "✓ Custom parameters work correctly\n";
}

// Test that IntervalCovering accepts the generated intervals
void test_interval_covering_compatibility() {
  const size_t n = 1000;
  auto intervals = test_utils::generate_intervals(n);

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  // This should not throw or assert in DEBUG mode
  IntervalCovering solver(n, getL, getR);
  solver.Run();

  // Count selected intervals
  size_t selected = 0;
  for (size_t i = 0; i < n; i++) {
    if (solver.valid[i]) {
      selected++;
    }
  }

  assert(selected > 0);
  assert(selected <= n);

  std::cout << "✓ IntervalCovering accepts generated intervals\n";
  std::cout << "  - Selected " << selected << " / " << n << " intervals\n";
}

int main() {
  std::cout << "Testing generate_intervals implementation\n";
  std::cout << "==========================================\n\n";

  test_interval_constraints();
  test_seed_variation();
  test_seed_reproducibility();
  test_custom_parameters();
  test_interval_covering_compatibility();

  std::cout << "\n==========================================\n";
  std::cout << "All tests passed!\n";

  return 0;
}
