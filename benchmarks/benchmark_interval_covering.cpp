#include "interval_covering.h"
#include "test_utils.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace std::chrono;

struct BenchmarkResult {
  size_t n;
  size_t num_selected_serial;
  size_t num_selected_parallel;
  double time_serial_ms;
  double time_parallel_ms;
  double speedup;
};

BenchmarkResult run_benchmark(size_t n) {
  auto intervals = test_utils::generate_intervals(n);

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  BenchmarkResult result;
  result.n = n;

  // Test Serial Version
  {
    IntervalCovering solver(intervals.size(), getL, getR);
    solver.valid = parlay::sequence<bool>(n, 0);

    auto start = high_resolution_clock::now();
    solver.KernelSerial();
    auto end = high_resolution_clock::now();

    result.num_selected_serial = 0;
    for (auto v : solver.valid) {
      result.num_selected_serial += v;
    }
    result.time_serial_ms =
        duration_cast<microseconds>(end - start).count() / 1000.0;
  }

  // Test Parallel Version
  {
    IntervalCovering solver(intervals.size(), getL, getR);

    auto start = high_resolution_clock::now();
    solver.Run();
    auto end = high_resolution_clock::now();

    result.num_selected_parallel = 0;
    for (auto v : solver.valid) {
      result.num_selected_parallel += v;
    }
    result.time_parallel_ms =
        duration_cast<microseconds>(end - start).count() / 1000.0;
  }

  // Verify both produce same results
  if (result.num_selected_serial != result.num_selected_parallel) {
    std::cerr << "ERROR: Serial and parallel produce different results!\n";
    std::cerr << "  Serial: " << result.num_selected_serial
              << ", Parallel: " << result.num_selected_parallel << "\n";
  }

  result.speedup = result.time_serial_ms / result.time_parallel_ms;

  return result;
}

int main() {
  std::cout << "Interval Covering Performance Benchmark\n";
  std::cout << "Serial vs Parallel Comparison\n";
  std::cout << "========================================\n\n";

  // Test sizes
  parlay::sequence<size_t> sizes = {1000,   2000,    5000,    10000,   20000,
                                50000,  100000,  200000,  500000,  1000000,
                                2000000, 5000000, 10000000};

  parlay::sequence<BenchmarkResult> results;

  std::cout << std::setw(12) << "N" << std::setw(12) << "Serial(ms)"
            << std::setw(12) << "Parallel(ms)" << std::setw(12) << "Speedup"
            << std::setw(15) << "Serial(M/s)" << std::setw(15) << "Parallel(M/s)"
            << "\n";
  std::cout << std::string(78, '-') << "\n";

  for (size_t n : sizes) {
    std::cout << "Running n=" << n << "..." << std::flush;
    auto result = run_benchmark(n);
    results.push_back(result);

    double throughput_serial = n / (result.time_serial_ms / 1000.0) / 1000000.0;
    double throughput_parallel =
        n / (result.time_parallel_ms / 1000.0) / 1000000.0;

    std::cout << "\r";  // Clear the line
    std::cout << std::setw(12) << n << std::setw(12) << std::fixed
              << std::setprecision(2) << result.time_serial_ms << std::setw(12)
              << result.time_parallel_ms << std::setw(12) << std::setprecision(2)
              << result.speedup << std::setw(15) << std::setprecision(1)
              << throughput_serial << std::setw(15) << throughput_parallel
              << std::endl;

    // Stop if taking too long
    if (result.time_parallel_ms > 10000) {
      std::cout << "\nBenchmark taking too long, stopping early.\n";
      break;
    }
  }

  // Write results to CSV
  std::ofstream csv("benchmark_parallel_comparison.csv");
  csv << "n,time_serial_ms,time_parallel_ms,speedup,selected\n";
  for (const auto& result : results) {
    csv << result.n << "," << result.time_serial_ms << ","
        << result.time_parallel_ms << "," << result.speedup << ","
        << result.num_selected_parallel << "\n";
  }
  csv.close();

  // Calculate average speedup
  double avg_speedup = 0;
  for (const auto& r : results) {
    avg_speedup += r.speedup;
  }
  avg_speedup /= results.size();

  std::cout << "\n========================================\n";
  std::cout << "Average Speedup: " << std::fixed << std::setprecision(2)
            << avg_speedup << "x\n";
  std::cout << "Results saved to benchmark_parallel_comparison.csv\n";

  return 0;
}
