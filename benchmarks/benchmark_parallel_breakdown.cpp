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

struct BreakdownResult {
  size_t n;
  size_t threads;
  double find_furthest_ms;
  double build_linklist_ms;
  double scan_linklist_ms;
  double extract_valid_ms;
  double total_ms;
};

// Run KernelParallel with timing for each phase
template<typename GetL, typename GetR>
BreakdownResult RunKernelParallelWithTiming(IntervalCovering<GetL, GetR>& solver) {
  BreakdownResult result;
  result.n = solver.n;
  result.threads = parlay::num_workers();

  auto start_total = high_resolution_clock::now();

  // Phase 1: BuildFurthest
  auto start = high_resolution_clock::now();
  solver.BuildFurthest();
  auto end = high_resolution_clock::now();
  result.find_furthest_ms = duration_cast<microseconds>(end - start).count() / 1000.0;

  // Phase 2: BuildLinkList
  start = high_resolution_clock::now();
  solver.BuildLinkList();
  end = high_resolution_clock::now();
  result.build_linklist_ms = duration_cast<microseconds>(end - start).count() / 1000.0;

  // Phase 3: ScanLinkList
  start = high_resolution_clock::now();
  solver.ScanLinkList();
  end = high_resolution_clock::now();
  result.scan_linklist_ms = duration_cast<microseconds>(end - start).count() / 1000.0;

  // Phase 4: Extract valid flags (the parallel_for at the end of KernelParallel)
  start = high_resolution_clock::now();
  parlay::parallel_for(0, solver.n, [&](size_t i) {
    solver.valid[i] = (solver.l_node(i).get_valid() != solver.r_node(i).get_valid()) ? 1 : 0;
  });
  end = high_resolution_clock::now();
  result.extract_valid_ms = duration_cast<microseconds>(end - start).count() / 1000.0;

  auto end_total = high_resolution_clock::now();
  result.total_ms = duration_cast<microseconds>(end_total - start_total).count() / 1000.0;

  return result;
}

BreakdownResult run_breakdown_benchmark(size_t n, int num_runs = 3) {
  auto intervals = test_utils::generate_intervals(n);
  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  parlay::sequence<BreakdownResult> results;

  for (int run = 0; run < num_runs; run++) {
    IntervalCovering solver(intervals.size(), getL, getR);
    solver.valid = parlay::sequence<bool>(n, 0);

    auto result = RunKernelParallelWithTiming(solver);
    results.push_back(result);
  }

  // Return median result based on total time
  std::sort(results.begin(), results.end(),
            [](const BreakdownResult& a, const BreakdownResult& b) {
              return a.total_ms < b.total_ms;
            });

  return results[num_runs / 2];
}

int main(int argc, char* argv[]) {
  std::cout << "Parallel Algorithm Breakdown Benchmark\n";
  std::cout << "=======================================\n\n";

  size_t current_threads = parlay::num_workers();
  std::cout << "Threads: " << current_threads << "\n\n";

  // Test configurations - can be overridden via command line
  parlay::sequence<size_t> sizes = {10000, 100000, 1000000, 10000000};

  // Parse command line arguments for custom sizes
  if (argc > 1) {
    sizes.clear();
    for (int i = 1; i < argc; i++) {
      sizes.push_back(std::stoull(argv[i]));
    }
  }

  parlay::sequence<BreakdownResult> results;

  std::cout << std::setw(12) << "N"
            << std::setw(14) << "BuildFurthest"
            << std::setw(12) << "BuildLink"
            << std::setw(12) << "ScanLink"
            << std::setw(14) << "ExtractValid"
            << std::setw(12) << "Total"
            << "\n";
  std::cout << std::string(76, '-') << "\n";

  for (size_t n : sizes) {
    std::cout << "Running n=" << n << "..." << std::flush;
    auto result = run_breakdown_benchmark(n);
    results.push_back(result);

    std::cout << "\r";
    std::cout << std::setw(12) << n
              << std::setw(14) << std::fixed << std::setprecision(2) << result.find_furthest_ms
              << std::setw(12) << result.build_linklist_ms
              << std::setw(12) << result.scan_linklist_ms
              << std::setw(14) << result.extract_valid_ms
              << std::setw(12) << result.total_ms
              << std::endl;

    if (result.total_ms > 30000) {
      std::cout << "\nBenchmark taking too long, stopping.\n";
      break;
    }
  }

  // Append results to CSV (allows multiple runs with different thread counts)
  bool file_exists = std::ifstream("parallel_breakdown.csv").good();
  std::ofstream csv("parallel_breakdown.csv", std::ios::app);

  if (!file_exists) {
    csv << "n,threads,find_furthest_ms,build_linklist_ms,scan_linklist_ms,extract_valid_ms,total_ms\n";
  }

  for (const auto& r : results) {
    csv << r.n << "," << r.threads << ","
        << std::fixed << std::setprecision(4)
        << r.find_furthest_ms << ","
        << r.build_linklist_ms << ","
        << r.scan_linklist_ms << ","
        << r.extract_valid_ms << ","
        << r.total_ms << "\n";
  }
  csv.close();

  std::cout << "\n=======================================\n";
  std::cout << "Results appended to parallel_breakdown.csv\n";

  return 0;
}
