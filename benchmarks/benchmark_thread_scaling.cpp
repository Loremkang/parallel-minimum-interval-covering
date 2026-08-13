#include "interval_covering.h"
#include "test_utils.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "parlay/parallel.h"

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kNumRuns = 3;
constexpr char kCsvHeader[] =
    "implementation,n,target_cover_size,actual_cover_size,threads,time_ms,"
    "throughput_M_per_sec";

struct BenchmarkInstance {
  size_t n;
  size_t target_cover_size;
};

struct BenchmarkResult {
  std::string implementation;
  size_t n;
  size_t target_cover_size;
  size_t actual_cover_size;
  size_t threads;
  double time_ms;
  double throughput_m_per_sec;
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

template <typename Run>
BenchmarkResult measure(std::string implementation,
                        const BenchmarkInstance& instance, size_t threads,
                        Run&& run) {
  std::vector<double> times;
  times.reserve(kNumRuns);

  size_t num_selected = 0;
  for (int repetition = 0; repetition < kNumRuns; ++repetition) {
    const auto start = Clock::now();
    auto selected = run();
    const auto end = Clock::now();

    const size_t count = static_cast<size_t>(
        std::count(selected.begin(), selected.end(), true));
    if (repetition == 0) {
      num_selected = count;
    } else if (count != num_selected) {
      throw std::runtime_error(implementation +
                               " returned inconsistent results across runs");
    }

    times.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  std::sort(times.begin(), times.end());
  const double median_time = times[times.size() / 2];
  return {std::move(implementation),
          instance.n,
          instance.target_cover_size,
          num_selected,
          threads,
          median_time,
          static_cast<double>(instance.n) / (median_time * 1000.0)};
}

template <typename GetL, typename GetR>
BenchmarkResult measure_serial(const BenchmarkInstance& instance, GetL left,
                               GetR right) {
  return measure("serial", instance, 1, [&] {
    return interval_covering::minimum_interval_cover<
        interval_covering::Implementation::Serial>(
        instance.n, left, right);
  });
}

template <interval_covering::Implementation implementation, typename GetL,
          typename GetR>
BenchmarkResult measure_parallel(const char* name,
                                 const BenchmarkInstance& instance,
                                 size_t threads, GetL left, GetR right) {
  return measure(name, instance, threads, [&] {
    return interval_covering::minimum_interval_cover<implementation>(
        instance.n, left, right);
  });
}

void print_result(const BenchmarkResult& result) {
  std::cout << std::setw(14) << result.implementation << std::setw(12)
            << result.n << std::setw(12) << result.target_cover_size
            << std::setw(12) << result.actual_cover_size << std::setw(10)
            << result.threads << std::setw(14) << std::fixed
            << std::setprecision(3) << result.time_ms << std::setw(18)
            << std::setprecision(2) << result.throughput_m_per_sec << '\n';
}

void require_same_result(const BenchmarkResult& expected,
                         const BenchmarkResult& actual) {
  if (actual.actual_cover_size != expected.actual_cover_size) {
    throw std::runtime_error(
        "result mismatch for n=" + std::to_string(actual.n) +
        ", target=" + std::to_string(actual.target_cover_size) + ": " +
        expected.implementation + " selected " +
        std::to_string(expected.actual_cover_size) + ", but " +
        actual.implementation + " selected " +
        std::to_string(actual.actual_cover_size));
  }
}

void append_results(const std::vector<BenchmarkResult>& results) {
  bool write_header = true;
  {
    std::ifstream existing("thread_scaling.csv");
    if (existing.good()) {
      std::string header;
      std::getline(existing, header);
      if (header != kCsvHeader) {
        throw std::runtime_error(
            "thread_scaling.csv has an incompatible header; remove it before "
            "running this benchmark");
      }
      write_header = false;
    }
  }

  std::ofstream csv("thread_scaling.csv", std::ios::app);
  if (!csv) {
    throw std::runtime_error("failed to open thread_scaling.csv");
  }
  if (write_header) {
    csv << kCsvHeader << '\n';
  }
  for (const auto& result : results) {
    csv << result.implementation << ',' << result.n << ','
        << result.target_cover_size << ',' << result.actual_cover_size << ','
        << result.threads << ',' << std::fixed << std::setprecision(6)
        << result.time_ms << ',' << std::setprecision(2)
        << result.throughput_m_per_sec << '\n';
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  try {
    bool include_serial = true;
    bool has_custom_instances = false;
    std::vector<BenchmarkInstance> instances = {
        {100000, 100},       {100000, 10000},
        {1000000, 1000},     {1000000, 100000},
        {10000000, 10000},   {10000000, 1000000},
    };

    for (int i = 1; i < argc; ++i) {
      const std::string argument = argv[i];
      if (argument == "--skip-serial") {
        include_serial = false;
        continue;
      }
      if (!has_custom_instances) {
        instances.clear();
        has_custom_instances = true;
      }
      instances.push_back(parse_instance(argument));
    }

    const size_t threads = parlay::num_workers();
    std::cout << "Interval Covering Implementation Scaling Benchmark\n"
              << "Parlay workers: " << threads << "\n"
              << "Each measurement is the median of " << kNumRuns
              << " runs.\n\n"
              << std::setw(14) << "Implementation" << std::setw(12) << "N"
              << std::setw(12) << "Target" << std::setw(12) << "Actual"
              << std::setw(10) << "Threads" << std::setw(14) << "Time(ms)"
              << std::setw(18)
              << "Throughput(M/s)" << '\n'
              << std::string(90, '-') << '\n';

    std::vector<BenchmarkResult> results;
    results.reserve(instances.size() * (include_serial ? 4 : 3));

    for (const auto& instance : instances) {
      const auto intervals = test_utils::generate_intervals(
          instance.n, instance.target_cover_size);
      const auto left = [&](size_t i) { return intervals[i].first; };
      const auto right = [&](size_t i) { return intervals[i].second; };

      if (include_serial) {
        auto serial = measure_serial(instance, left, right);
        print_result(serial);
        results.push_back(std::move(serial));
      }

      auto fine_tuned = measure_parallel<
          interval_covering::Implementation::FineTuned>(
          "fine_tuned", instance, threads, left, right);
      print_result(fine_tuned);

      auto sampling = measure_parallel<
          interval_covering::Implementation::Sampling>(
          "sampling", instance, threads, left, right);
      print_result(sampling);

      auto euler_tour = measure_parallel<
          interval_covering::Implementation::EulerTour>(
          "euler_tour", instance, threads, left, right);
      print_result(euler_tour);

      require_same_result(fine_tuned, sampling);
      require_same_result(sampling, euler_tour);
      if (include_serial) {
        require_same_result(results.back(), fine_tuned);
      }

      const double slowest_parallel = std::max(
          {fine_tuned.time_ms, sampling.time_ms, euler_tour.time_ms});
      results.push_back(std::move(fine_tuned));
      results.push_back(std::move(sampling));
      results.push_back(std::move(euler_tour));

      if (slowest_parallel > 30000.0) {
        std::cout << "A parallel implementation exceeded 30 seconds; "
                     "stopping before the next size.\n";
        break;
      }
    }

    append_results(results);
    std::cout << "\nResults appended to thread_scaling.csv\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ERROR: " << error.what() << '\n';
    return 1;
  }
}
