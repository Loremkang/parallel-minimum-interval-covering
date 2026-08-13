#ifndef INTERVAL_COVERING_INTERNAL_SAMPLING_H
#define INTERVAL_COVERING_INTERNAL_SAMPLING_H

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

#include "interval_covering/internal/common.h"
#include "parlay/parallel.h"
#include "parlay/primitives.h"
#include "parlay/random.h"

namespace interval_covering {
namespace internal {
namespace sampling {

template <typename GetL, typename GetR>
class Solver {
 public:
  Solver(size_t n, GetL left, GetR right)
      : n(n), left(std::move(left)), right(std::move(right)) {}

  void BuildFurthest() {
    furthest_id = common::build_furthest(n, left, right);
  }

  void BuildIntervalSample() {
    const size_t sample_rate = parallel_block_size;
    parlay::random random(0);
    sampled = parlay::tabulate(n, [&](size_t i) -> bool {
      return random.ith_rand(i) % sample_rate == 0;
    });
    sampled[0] = sampled[n - 1] = true;
    sampled_id = parlay::pack_index(sampled);
  }

  // For each sampled interval, find the next sampled interval on the optimal
  // furthest-interval path.
  void BuildConnectionBetweenSamples() {
    sampled_id_nxt_initial = parlay::sequence<size_t>(n, 0);
    parlay::parallel_for(0, sampled_id.size(), [&](size_t i) {
      const size_t start_id = sampled_id[i];
      size_t id = furthest_id[start_id];
      while (!sampled[id]) {
        id = furthest_id[id];
      }
      sampled_id_nxt_initial[start_id] = id;
    });
  }

  void ScanSamples() {
    valid_sampled_node.clear();
    size_t id = 0;
    while (id < n - 1) {
      valid[id] = true;
      valid_sampled_node.push_back(id);
      id = sampled_id_nxt_initial[id];
    }
    valid[id] = true;
  }

  void ScanNonsampleNodes() {
    parlay::parallel_for(0, valid_sampled_node.size(), [&](size_t i) {
      const size_t start_id = valid_sampled_node[i];
      const size_t end_id = sampled_id_nxt_initial[start_id];

      size_t id = furthest_id[start_id];
      while (id != end_id) {
        assert(id < end_id);
        valid[id] = true;
        id = furthest_id[id];
      }
    });
  }

  void KernelParallel() {
    BuildFurthest();
    BuildIntervalSample();
    BuildConnectionBetweenSamples();
    ScanSamples();
    ScanNonsampleNodes();
  }

  // Retained for serial-vs-parallel benchmarks; not part of the public API.
  void KernelSerial() {
    common::serial_minimum_interval_cover(n, left, right, valid);
  }

  void Run() {
    valid = parlay::sequence<bool>(n, false);
    if (n == 0) return;
    if (n <= 2) {
      valid[0] = true;
      valid[n - 1] = true;
      return;
    }
    KernelParallel();
  }

  static constexpr size_t parallel_block_size = parlay::internal::_block_size;

  size_t n;
  GetL left;
  GetR right;

  std::vector<size_t> valid_sampled_node;
  parlay::sequence<bool> valid;
  parlay::sequence<bool> sampled;
  parlay::sequence<size_t> furthest_id;
  parlay::sequence<size_t> sampled_id;
  parlay::sequence<size_t> sampled_id_nxt_initial;
};

template <typename GetL, typename GetR>
parlay::sequence<bool> solve(size_t n, GetL left, GetR right) {
  Solver<GetL, GetR> solver(n, std::move(left), std::move(right));
  solver.Run();
  return std::move(solver.valid);
}

}  // namespace sampling
}  // namespace internal
}  // namespace interval_covering

#endif
