#include "interval_covering.h"
#include "test_utils.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>
#include "parlay/parallel.h"

using namespace std::chrono;

struct BenchmarkResult {
  std::string algorithm;
  size_t n;
  size_t threads;
  double time_ms;
  size_t num_selected;
  double throughput_M_per_sec;
};

// Run serial baseline
BenchmarkResult run_serial_benchmark(size_t n, int num_runs = 3) {
  auto intervals = test_utils::generate_intervals(n);
  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  std::vector<double> times;
  size_t num_selected = 0;

  for (int run = 0; run < num_runs; run++) {
    IntervalCovering solver(intervals.size(), getL, getR);
    solver.valid = parlay::sequence<bool>(n, 0);

    auto start = high_resolution_clock::now();
    solver.KernelSerial();
    auto end = high_resolution_clock::now();

    double time_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    times.push_back(time_ms);

    if (run == 0) {
      for (auto v : solver.valid) {
        num_selected += v;
      }
    }
  }

  // Take median time
  std::sort(times.begin(), times.end());
  double median_time = times[num_runs / 2];

  BenchmarkResult result;
  result.algorithm = "serial";
  result.n = n;
  result.threads = 1;
  result.time_ms = median_time;
  result.num_selected = num_selected;
  result.throughput_M_per_sec = n / (median_time / 1000.0) / 1000000.0;

  return result;
}

// Run parallel benchmark (uses PARLAY_NUM_THREADS env var for thread count)
BenchmarkResult run_parallel_benchmark(size_t n, int num_runs = 3) {
  auto intervals = test_utils::generate_intervals(n);
  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  std::vector<double> times;
  size_t num_selected = 0;

  // Get actual thread count from parlay
  size_t actual_threads = parlay::num_workers();

  for (int run = 0; run < num_runs; run++) {
    IntervalCovering solver(intervals.size(), getL, getR);

    auto start = high_resolution_clock::now();
    solver.Run();
    auto end = high_resolution_clock::now();

    double time_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    times.push_back(time_ms);

    if (run == 0) {
      for (auto v : solver.valid) {
        num_selected += v;
      }
    }
  }

  // Take median time
  std::sort(times.begin(), times.end());
  double median_time = times[num_runs / 2];

  BenchmarkResult result;
  result.algorithm = "parallel";
  result.n = n;
  result.threads = actual_threads;
  result.time_ms = median_time;
  result.num_selected = num_selected;
  result.throughput_M_per_sec = n / (median_time / 1000.0) / 1000000.0;

  return result;
}

void print_result(const BenchmarkResult& r) {
  std::cout << std::setw(12) << r.algorithm
            << std::setw(12) << r.n
            << std::setw(10) << r.threads
            << std::setw(14) << std::fixed << std::setprecision(2) << r.time_ms
            << std::setw(12) << r.num_selected
            << std::setw(18) << std::setprecision(1) << r.throughput_M_per_sec
            << std::endl;
}

int main(int argc, char* argv[]) {
  std::cout << "Thread Scaling Performance Benchmark\n";
  std::cout << "=====================================\n\n";

  // Get current thread count from ParlayLib (set via PARLAY_NUM_THREADS env var)
  size_t current_threads = parlay::num_workers();
  std::cout << "Current PARLAY_NUM_THREADS: " << current_threads << "\n\n";

  // Test configurations - can be overridden via command line
  std::vector<size_t> sizes = {10000, 100000, 1000000, 10000000};

  // Parse command line arguments for custom sizes
  if (argc > 1) {
    sizes.clear();
    for (int i = 1; i < argc; i++) {
      sizes.push_back(std::stoull(argv[i]));
    }
  }

  std::vector<BenchmarkResult> results;

  // Print header
  std::cout << std::setw(12) << "Algorithm"
            << std::setw(12) << "N"
            << std::setw(10) << "Threads"
            << std::setw(14) << "Time(ms)"
            << std::setw(12) << "Selected"
            << std::setw(18) << "Throughput(M/s)"
            << "\n";
  std::cout << std::string(78, '-') << "\n";

  for (size_t n : sizes) {
    // Run serial baseline
    std::cout << "Running serial (n=" << n << ")..." << std::flush;
    auto serial_result = run_serial_benchmark(n);
    results.push_back(serial_result);
    std::cout << "\r";
    print_result(serial_result);

    // Run parallel
    std::cout << "Running parallel (n=" << n << ", threads=" << current_threads << ")..." << std::flush;
    auto parallel_result = run_parallel_benchmark(n);
    results.push_back(parallel_result);
    std::cout << "\r";
    print_result(parallel_result);

    // Early stop if taking too long (> 30 seconds)
    if (parallel_result.time_ms > 30000) {
      std::cout << "\nBenchmark taking too long, stopping early.\n";
      break;
    }
  }

  // Append results to CSV (allows multiple runs with different thread counts)
  bool file_exists = std::ifstream("thread_scaling_results.csv").good();
  std::ofstream csv("thread_scaling_results.csv", std::ios::app);

  if (!file_exists) {
    csv << "algorithm,n,threads,time_ms,num_selected,throughput_M_per_sec\n";
  }

  for (const auto& result : results) {
    csv << result.algorithm << ","
        << result.n << ","
        << result.threads << ","
        << std::fixed << std::setprecision(4) << result.time_ms << ","
        << result.num_selected << ","
        << std::setprecision(2) << result.throughput_M_per_sec << "\n";
  }
  csv.close();

  std::cout << "\n=====================================\n";
  std::cout << "Results appended to thread_scaling_results.csv\n";
  std::cout << "Run with different PARLAY_NUM_THREADS values to collect more data\n";
  std::cout << "Then run plot_performance.py to generate visualizations\n";

  return 0;
}
