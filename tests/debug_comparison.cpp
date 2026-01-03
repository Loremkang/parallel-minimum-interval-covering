#include "interval_covering.h"
#include "test_utils.h"
#include <iostream>
#include <random>
#include <vector>

int main() {
  size_t n = 20;
  auto intervals = test_utils::generate_intervals(n);

  std::cout << "Generated intervals:\n";
  for (size_t i = 0; i < n; i++) {
    std::cout << "  " << i << ": [" << intervals[i].first << ", "
              << intervals[i].second << "]\n";
  }

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  // Test Serial
  parlay::sequence<size_t> serial_selected;
  {
    IntervalCovering solver(intervals.size(), getL, getR);
    solver.valid = parlay::sequence<bool>(n, 0);
    solver.KernelSerial();
    for (size_t i = 0; i < n; i++) {
      if (solver.valid[i]) {
        serial_selected.push_back(i);
      }
    }
  }

  // Test Parallel
  std::vector<size_t> parallel_selected;
  {
    IntervalCovering solver(intervals.size(), getL, getR);
    solver.Run();
    for (size_t i = 0; i < n; i++) {
      if (solver.valid[i]) {
        parallel_selected.push_back(i);
      }
    }
  }

  std::cout << "\nSerial selected " << serial_selected.size()
            << " intervals: ";
  for (auto idx : serial_selected) {
    std::cout << idx << " ";
  }
  std::cout << "\n";

  std::cout << "Parallel selected " << parallel_selected.size()
            << " intervals: ";
  for (auto idx : parallel_selected) {
    std::cout << idx << " ";
  }
  std::cout << "\n";

  return 0;
}
