#include "interval_covering.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

// Test helper functions
template <typename T>
void print_intervals(const std::vector<std::pair<T, T>>& intervals) {
  for (size_t i = 0; i < intervals.size(); i++) {
    std::cout << "  [" << i << "]: (" << intervals[i].first << ", "
              << intervals[i].second << ")\n";
  }
}

template <typename T>
void print_result(const std::vector<std::pair<T, T>>& intervals,
                  const parlay::sequence<bool>& valid) {
  std::cout << "Minimum interval cover:\n";
  for (size_t i = 0; i < valid.size(); i++) {
    if (valid[i]) {
      std::cout << "  Interval " << i << ": (" << intervals[i].first << ", "
                << intervals[i].second << ")\n";
    }
  }
}

// Verify that the selected intervals form a valid cover
template <typename T>
bool verify_cover(const std::vector<std::pair<T, T>>& intervals,
                  const parlay::sequence<bool>& valid, T target = 0) {
  parlay::sequence<size_t> selected = parlay::pack_index(valid);
  // for (size_t i = 0; i < valid.size(); i++) {
  //   if (valid[i]) {
  //     selected.push_back(i);
  //   }
  // }

  if (selected.empty()) {
    std::cerr << "ERROR: No intervals selected\n";
    return false;
  }

  // Check that intervals cover from target point
  if (intervals[selected[0]].first > target) {
    std::cerr << "ERROR: First interval doesn't cover target " << target
              << "\n";
    return false;
  }

  // Check continuity
  for (size_t i = 0; i < selected.size() - 1; i++) {
    T current_right = intervals[selected[i]].second;
    T next_left = intervals[selected[i + 1]].first;
    if (next_left > current_right) {
      std::cerr << "ERROR: Gap between interval " << selected[i] << " and "
                << selected[i + 1] << "\n";
      return false;
    }
  }

  return true;
}

// Test 1: Simple small case
template <typename T>
void test_simple() {
  std::cout << "\n=== Test 1: Simple Case (type " << typeid(T).name() << ") ===\n";
  std::vector<std::pair<T, T>> intervals = {{0, 5},   {1, 8},   {3, 10},
                                                 {7, 15},  {12, 20}, {18, 25},
                                                 {22, 30}, {28, 35}};

  std::cout << "Input intervals:\n";
  print_intervals(intervals);

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  IntervalCovering solver(intervals.size(), getL, getR);
  solver.Run();

  print_result(intervals, solver.valid);
  assert(verify_cover(intervals, solver.valid));

  // Expected: intervals that extend coverage the most
  // Should include interval 0 (covers 0-5), then one that reaches furthest
  // from 0-5, etc.
  size_t count = 0;
  for (auto v : solver.valid) count += v;
  std::cout << "Selected " << count << " intervals\n";

  std::cout << "PASSED\n";
}

// Test 2: Single interval
template <typename T>
void test_single_interval() {
  std::cout << "\n=== Test 2: Single Interval (type " << typeid(T).name() << ") ===\n";
  std::vector<std::pair<T, T>> intervals = {{0, 10}};

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  IntervalCovering solver(intervals.size(), getL, getR);
  solver.Run();

  assert(solver.valid[0] == 1);
  std::cout << "PASSED\n";
}

// Test 3: Two intervals
template <typename T>
void test_two_intervals() {
  std::cout << "\n=== Test 3: Two Intervals (type " << typeid(T).name() << ") ===\n";
  std::vector<std::pair<T, T>> intervals = {{0, 5}, {3, 10}};

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  IntervalCovering solver(intervals.size(), getL, getR);
  solver.Run();

  print_result(intervals, solver.valid);
  assert(verify_cover(intervals, solver.valid));
  std::cout << "PASSED\n";
}

// Test 4: Adjacent intervals (touching but not overlapping)
template <typename T>
void test_non_overlapping() {
  std::cout << "\n=== Test 4: Adjacent Intervals (type " << typeid(T).name() << ") ===\n";
  // Must satisfy L(i+1) <= R(i), so make them touch
  std::vector<std::pair<T, T>> intervals = {
      {0, 5}, {5, 10}, {10, 15}, {15, 20}};

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  IntervalCovering solver(intervals.size(), getL, getR);
  solver.Run();

  print_result(intervals, solver.valid);

  // Adjacent intervals should all be selected
  size_t count = 0;
  for (auto v : solver.valid) count += v;
  std::cout << "Selected " << count << " intervals (expected all)\n";

  std::cout << "PASSED\n";
}

// Test 5: Nested/Overlapping intervals with monotonic L and R
template <typename T>
void test_nested() {
  std::cout << "\n=== Test 5: Nested/Overlapping Intervals (type " << typeid(T).name() << ") ===\n";
  // Must satisfy: L(i) < L(i+1) AND R(i) < R(i+1)
  std::vector<std::pair<T, T>> intervals = {{0, 50}, {10, 60}, {15, 70},
                                                 {30, 80}, {35, 90}};

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  IntervalCovering solver(intervals.size(), getL, getR);
  solver.Run();

  print_result(intervals, solver.valid);
  assert(verify_cover(intervals, solver.valid));

  std::cout << "PASSED\n";
}

// Test 6: Many overlapping intervals
template <typename T>
void test_many_overlapping() {
  std::cout << "\n=== Test 6: Many Overlapping Intervals (type " << typeid(T).name() << ") ===\n";
  std::vector<std::pair<T, T>> intervals;
  for (T i = 0; i < 50; i++) {
    intervals.push_back({i * 2, i * 2 + 10});
  }

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  IntervalCovering solver(intervals.size(), getL, getR);
  solver.Run();

  assert(verify_cover(intervals, solver.valid));

  size_t count = 0;
  for (auto v : solver.valid) count += v;
  std::cout << "Selected " << count << " out of " << intervals.size()
            << " intervals\n";

  std::cout << "PASSED\n";
}

// Test 7: Large random test
template <typename T>
void test_large_random() {
  std::cout << "\n=== Test 7: Large Random Test (type " << typeid(T).name() << ") ===\n";
  const size_t n = 10000;
  std::vector<std::pair<T, T>> intervals;

  // Generate random intervals with strict monotonicity
  // Must ensure L(i) < L(i+1) and R(i) < R(i+1)
  T left = 0;
  T right = 10;
  for (size_t i = 0; i < n; i++) {
    intervals.push_back({left, right});
    left += (rand() % 5) + 1;    // +1 ensures strict increase
    right += (rand() % 5) + 4;   // +4 ensures strict increase
  }

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  IntervalCovering solver(intervals.size(), getL, getR);
  solver.Run();

  assert(verify_cover(intervals, solver.valid));

  size_t count = 0;
  for (auto v : solver.valid) count += v;
  std::cout << "Selected " << count << " out of " << intervals.size()
            << " intervals\n";

  std::cout << "PASSED\n";
}

// Test 8: Edge case with very similar (nearly identical) intervals
template <typename T>
void test_identical_intervals() {
  std::cout << "\n=== Test 8: Very Similar Intervals (type " << typeid(T).name() << ") ===\n";
  // Cannot have identical L or R values due to strict monotonicity requirement
  std::vector<std::pair<T, T>> intervals = {
      {0, 10}, {5, 15}, {6, 16}, {7, 17}, {10, 20}};

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  IntervalCovering solver(intervals.size(), getL, getR);
  solver.Run();

  print_result(intervals, solver.valid);
  assert(verify_cover(intervals, solver.valid));

  std::cout << "PASSED\n";
}

// Test 9: Very long chain
template <typename T>
void test_long_chain() {
  std::cout << "\n=== Test 9: Long Chain (type " << typeid(T).name() << ") ===\n";
  const size_t n = 1000;
  std::vector<std::pair<T, T>> intervals;

  for (size_t i = 0; i < n; i++) {
    intervals.push_back({static_cast<T>(i), static_cast<T>(i + 2)});
  }

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  IntervalCovering solver(intervals.size(), getL, getR);
  solver.Run();

  assert(verify_cover(intervals, solver.valid));

  size_t count = 0;
  for (auto v : solver.valid) count += v;
  std::cout << "Selected " << count << " out of " << intervals.size()
            << " intervals\n";

  std::cout << "PASSED\n";
}

// Test 10: Non-strict monotonic intervals (L(i) == L(i+1) or R(i) == R(i+1))
template <typename T>
void test_non_strict_monotonic() {
  std::cout << "\n=== Test 10: Non-Strict Monotonic Intervals (type " << typeid(T).name() << ") ===\n";

  // Test case 1: Equal left endpoints (L(i) == L(i+1))
  {
    std::cout << "  Case 1: Equal left endpoints...\n";
    std::vector<std::pair<T, T>> intervals = {
        {0, 10}, {0, 15}, {0, 20}, {5, 25}, {5, 30}};

    auto getL = [&](size_t i) { return intervals[i].first; };
    auto getR = [&](size_t i) { return intervals[i].second; };

    IntervalCovering solver(intervals.size(), getL, getR);
    solver.Run();

    print_result(intervals, solver.valid);
    assert(verify_cover(intervals, solver.valid));
  }

  // Test case 2: Equal right endpoints (R(i) == R(i+1))
  {
    std::cout << "  Case 2: Equal right endpoints...\n";
    std::vector<std::pair<T, T>> intervals = {
        {0, 20}, {5, 20}, {10, 20}, {15, 30}, {20, 30}};

    auto getL = [&](size_t i) { return intervals[i].first; };
    auto getR = [&](size_t i) { return intervals[i].second; };

    IntervalCovering solver(intervals.size(), getL, getR);
    solver.Run();

    print_result(intervals, solver.valid);
    assert(verify_cover(intervals, solver.valid));
  }

  // Test case 3: Both equal (L(i) == L(i+1) AND R(i) == R(i+1) for some i)
  {
    std::cout << "  Case 3: Identical consecutive intervals...\n";
    std::vector<std::pair<T, T>> intervals = {
        {0, 10}, {0, 10}, {0, 10}, {5, 20}, {5, 20}, {10, 25}};

    auto getL = [&](size_t i) { return intervals[i].first; };
    auto getR = [&](size_t i) { return intervals[i].second; };

    IntervalCovering solver(intervals.size(), getL, getR);
    solver.Run();

    print_result(intervals, solver.valid);
    assert(verify_cover(intervals, solver.valid));
  }

  // Test case 4: Large test with many equal endpoints
  {
    std::cout << "  Case 4: Large non-strict monotonic test...\n";
    std::vector<std::pair<T, T>> intervals;
    T left = 0;
    T right = 10;

    for (size_t i = 0; i < 1000; i++) {
      intervals.push_back({left, right});
      // Sometimes keep same left or right (50% chance each)
      if (rand() % 2 == 0) {
        left += (rand() % 3);  // May be 0
      }
      if (rand() % 2 == 0) {
        right += (rand() % 5);  // May be 0
      } else {
        right += (rand() % 5) + 1;  // Ensure some progress
      }
      // Ensure R(i) > L(i) and no gaps
      if (right <= left) right = left + 1;
    }

    auto getL = [&](size_t i) { return intervals[i].first; };
    auto getR = [&](size_t i) { return intervals[i].second; };

    IntervalCovering solver(intervals.size(), getL, getR);
    solver.Run();

    assert(verify_cover(intervals, solver.valid));

    size_t count = 0;
    for (auto v : solver.valid) count += v;
    std::cout << "  Selected " << count << " out of " << intervals.size()
              << " intervals\n";
  }

  std::cout << "PASSED\n";
}

// Test 11: Stress test with various sizes
template <typename T>
void test_various_sizes() {
  std::cout << "\n=== Test 11: Various Sizes (type " << typeid(T).name() << ") ===\n";

  parlay::sequence<size_t> sizes = {1,   2,   3,    5,    10,   50,
                                100, 500, 1000, 5000, 10000};

  for (size_t n : sizes) {
    std::vector<std::pair<T, T>> intervals;
    T left = 0;
    T right = 5;

    for (size_t i = 0; i < n; i++) {
      intervals.push_back({left, right});
      left += (rand() % 3) + 1;   // +1 ensures strict increase
      right += (rand() % 4) + 3;  // +3 ensures strict increase
    }

    auto getL = [&](size_t i) { return intervals[i].first; };
    auto getR = [&](size_t i) { return intervals[i].second; };

    IntervalCovering solver(intervals.size(), getL, getR);
    solver.Run();

    assert(verify_cover(intervals, solver.valid));

    size_t count = 0;
    for (auto v : solver.valid) count += v;
    std::cout << "  n=" << n << ": selected " << count << " intervals\n";
  }

  std::cout << "PASSED\n";
}

template <typename T>
void run_all_tests() {
  test_simple<T>();
  test_single_interval<T>();
  test_two_intervals<T>();
  test_non_overlapping<T>();
  test_nested<T>();
  test_many_overlapping<T>();
  test_large_random<T>();
  test_identical_intervals<T>();
  test_long_chain<T>();
  test_non_strict_monotonic<T>();
  test_various_sizes<T>();
}

int main() {
  std::cout << "Running Interval Covering Tests\n";
  std::cout << "================================\n";

  try {
    std::cout << "\n### Testing with type: int ###\n";
    run_all_tests<int>();

    std::cout << "\n### Testing with type: int64_t ###\n";
    run_all_tests<int64_t>();

    std::cout << "\n================================\n";
    std::cout << "ALL TESTS PASSED!\n";
    std::cout << "================================\n";

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Test failed with exception: " << e.what() << "\n";
    return 1;
  } catch (...) {
    std::cerr << "Test failed with unknown exception\n";
    return 1;
  }
}
