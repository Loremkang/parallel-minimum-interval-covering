#include "interval_covering.h"
#include "test_utils.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std::chrono;

struct BreakdownResult {
  size_t n;
  size_t target_cover_size;
  size_t actual_cover_size;
  size_t threads;
  double build_furthest_ms;
  double sample_intervals_ms;
  double build_connections_ms;
  double scan_samples_ms;
  double scan_nonsample_ms;
  double total_ms;
};

struct BenchmarkInstance {
  size_t n;
  size_t target_cover_size;
};

BenchmarkInstance parse_instance(const std::string& argument) {
  const size_t separator = argument.find(':');
  if (separator == std::string::npos || separator == 0 ||
      separator + 1 == argument.size() ||
      argument.find(':', separator + 1) != std::string::npos) {
    throw std::invalid_argument(
        "benchmark instances must use n:target_cover_size");
  }
  size_t n_consumed = 0;
  size_t target_consumed = 0;
  const std::string n_text = argument.substr(0, separator);
  const std::string target_text = argument.substr(separator + 1);
  const size_t n = std::stoull(n_text, &n_consumed);
  const size_t target = std::stoull(target_text, &target_consumed);
  if (n_consumed != n_text.size() || target_consumed != target_text.size()) {
    throw std::invalid_argument(
        "benchmark instances must use n:target_cover_size");
  }
  return {n, target};
}

// Run KernelParallelFast with timing for each phase
template<typename GetL, typename GetR>
BreakdownResult RunKernelParallelWithTiming(
    interval_covering::internal::sampling::Solver<GetL, GetR>& solver,
    size_t target_cover_size) {
  BreakdownResult result;
  result.n = solver.n;
  result.target_cover_size = target_cover_size;
  result.threads = parlay::num_workers();

  auto start_total = high_resolution_clock::now();

  // Phase 1: BuildFurthest
  auto start = high_resolution_clock::now();
  solver.BuildFurthest();
  auto end = high_resolution_clock::now();
  result.build_furthest_ms = duration_cast<microseconds>(end - start).count() / 1000.0;

  // Phase 2: BuildIntervalSample
  start = high_resolution_clock::now();
  solver.BuildIntervalSample();
  end = high_resolution_clock::now();
  result.sample_intervals_ms = duration_cast<microseconds>(end - start).count() / 1000.0;

  // Phase 3: BuildConnectionBetweenSamples
  start = high_resolution_clock::now();
  solver.BuildConnectionBetweenSamples();
  end = high_resolution_clock::now();
  result.build_connections_ms = duration_cast<microseconds>(end - start).count() / 1000.0;

  // Phase 4: ScanSamples
  start = high_resolution_clock::now();
  solver.ScanSamples();
  end = high_resolution_clock::now();
  result.scan_samples_ms = duration_cast<microseconds>(end - start).count() / 1000.0;

  // Phase 5: ScanNonsampleNodes
  start = high_resolution_clock::now();
  solver.ScanNonsampleNodes();
  end = high_resolution_clock::now();
  result.scan_nonsample_ms = duration_cast<microseconds>(end - start).count() / 1000.0;

  auto end_total = high_resolution_clock::now();
  result.total_ms = duration_cast<microseconds>(end_total - start_total).count() / 1000.0;
  result.actual_cover_size = static_cast<size_t>(
      std::count(solver.valid.begin(), solver.valid.end(), true));

  return result;
}

BreakdownResult run_breakdown_benchmark(const BenchmarkInstance& instance,
                                        int num_runs = 3) {
  auto intervals = test_utils::generate_intervals(
      instance.n, instance.target_cover_size);
  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  parlay::sequence<BreakdownResult> results;

  for (int run = 0; run < num_runs; run++) {
    interval_covering::internal::sampling::Solver solver(
        intervals.size(), getL, getR);
    solver.valid = parlay::sequence<bool>(instance.n, false);

    auto result = RunKernelParallelWithTiming(
        solver, instance.target_cover_size);
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
  std::vector<BenchmarkInstance> instances = {
      {100000, 100},       {100000, 10000},
      {1000000, 1000},     {1000000, 100000},
      {10000000, 10000},   {10000000, 1000000},
  };

  // Parse command-line n:target_cover_size instances.
  if (argc > 1) {
    instances.clear();
    for (int i = 1; i < argc; i++) {
      instances.push_back(parse_instance(argv[i]));
    }
  }

  parlay::sequence<BreakdownResult> results;

  std::cout << std::setw(12) << "N"
            << std::setw(12) << "Target"
            << std::setw(12) << "Actual"
            << std::setw(14) << "BuildFurthest"
            << std::setw(14) << "SampleInterv"
            << std::setw(14) << "BuildConnect"
            << std::setw(12) << "ScanSample"
            << std::setw(14) << "ScanNonsample"
            << std::setw(12) << "Total"
            << "\n";
  std::cout << std::string(116, '-') << "\n";

  for (const auto& instance : instances) {
    std::cout << "Running n=" << instance.n
              << ", target=" << instance.target_cover_size << "..."
              << std::flush;
    auto result = run_breakdown_benchmark(instance);
    results.push_back(result);

    std::cout << "\r";
    std::cout << std::setw(12) << result.n
              << std::setw(12) << result.target_cover_size
              << std::setw(12) << result.actual_cover_size
              << std::setw(14) << std::fixed << std::setprecision(2) << result.build_furthest_ms
              << std::setw(14) << result.sample_intervals_ms
              << std::setw(14) << result.build_connections_ms
              << std::setw(12) << result.scan_samples_ms
              << std::setw(14) << result.scan_nonsample_ms
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
    csv << "n,target_cover_size,actual_cover_size,threads,build_furthest_ms,"
           "sample_intervals_ms,build_connections_ms,scan_samples_ms,"
           "scan_nonsample_ms,total_ms\n";
  }

  for (const auto& r : results) {
    csv << r.n << "," << r.target_cover_size << ","
        << r.actual_cover_size << "," << r.threads << ","
        << std::fixed << std::setprecision(4)
        << r.build_furthest_ms << ","
        << r.sample_intervals_ms << ","
        << r.build_connections_ms << ","
        << r.scan_samples_ms << ","
        << r.scan_nonsample_ms << ","
        << r.total_ms << "\n";
  }
  csv.close();

  std::cout << "\n=======================================\n";
  std::cout << "Results appended to parallel_breakdown.csv\n";

  return 0;
}
