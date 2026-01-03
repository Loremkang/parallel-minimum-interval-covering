#include "interval_covering.h"
#include "test_utils.h"
#include <iostream>
#include <vector>

int main() {
  std::cout << "Testing benchmark flow (serial then parallel)\n";

  const size_t n = 1000;
  auto intervals = test_utils::generate_intervals(n);

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  // Test Serial Version (like benchmark)
  std::cout << "Running serial version...\n";
  std::cout.flush();
  {
    IntervalCovering solver(intervals.size(), getL, getR);
    solver.valid = parlay::sequence<bool>(n, 0);
    solver.KernelSerial();

    size_t count = 0;
    for (auto v : solver.valid) {
      count += v;
    }
    std::cout << "Serial selected " << count << " intervals\n";
  }

  // Test Parallel Version (like benchmark)
  std::cout << "Running parallel version...\n";
  std::cout.flush();
  {
    IntervalCovering solver(intervals.size(), getL, getR);
    solver.Run();

    size_t count = 0;
    for (auto v : solver.valid) {
      count += v;
    }
    std::cout << "Parallel selected " << count << " intervals\n";
  }

  std::cout << "Both versions completed!\n";

  return 0;
}
