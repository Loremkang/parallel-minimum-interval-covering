#ifndef INTERVAL_COVERING_INTERNAL_EULER_TOUR_H
#define INTERVAL_COVERING_INTERNAL_EULER_TOUR_H

#include <cassert>
#include <cstddef>
#include <utility>

#include "interval_covering/internal/common.h"
#include "parlay/parallel.h"
#include "parlay/primitives.h"
#include "parlay/random.h"

namespace interval_covering {
namespace internal {
namespace euler_tour {

// Legacy Euler-tour implementation retained as an alternative to sampling.
template <typename GetL, typename GetR>
class Solver {
 public:
  static constexpr size_t kNullPtr = (1ULL << 62) - 1;

  class LinkListNode {
   public:
    LinkListNode(bool valid = false, size_t next = kNullPtr,
                 bool sampled = false)
        : next_(next), sampled_(sampled), valid_(valid) {}

    bool get_valid() const { return valid_; }
    void set_valid(bool value) { valid_ = value; }
    bool get_sampled() const { return sampled_; }
    void set_sampled(bool value) { sampled_ = value; }
    size_t get_next() const { return next_; }
    void set_next(size_t next) {
      assert(next <= kNullPtr);
      next_ = next;
    }

   private:
    size_t next_;
    bool sampled_;
    bool valid_;
  };

  Solver(size_t n, GetL left, GetR right)
      : n(n), left(std::move(left)), right(std::move(right)) {}

  size_t left_node_id(size_t i) const { return i * 2; }
  size_t right_node_id(size_t i) const { return i * 2 + 1; }
  LinkListNode& left_node(size_t i) { return link_list[left_node_id(i)]; }
  LinkListNode& right_node(size_t i) { return link_list[right_node_id(i)]; }
  void link(size_t from, size_t to) { link_list[from].set_next(to); }

  void BuildFurthest() {
    furthest_id = common::build_furthest(n, left, right);
  }

  void BuildLinkList() {
    link_list = parlay::sequence<LinkListNode>(n * 2, LinkListNode());
    assert(n >= 2);

    // The final two furthest links are identical, so handle the tail explicitly
    // to avoid introducing a cycle.
    link(left_node_id(n - 2), right_node_id(n - 2));
    link(right_node_id(n - 2), right_node_id(n - 1));
    parlay::parallel_for(0, n - 2, [&](size_t i) {
      link(left_node_id(i), right_node_id(i));
      if (furthest_id[i] == furthest_id[i + 1]) {
        link(right_node_id(i), left_node_id(i + 1));
      } else {
        link(right_node_id(i), right_node_id(furthest_id[i]));
      }
    }, parallel_block_size);

    link(left_node_id(furthest_id[0]), left_node_id(0));
    parlay::parallel_for(1, n - 1, [&](size_t i) {
      if (furthest_id[i - 1] != furthest_id[i]) {
        link(left_node_id(furthest_id[i]), left_node_id(i));
      }
    }, parallel_block_size);

    right_node(0).set_valid(true);
  }

  void BuildSampleIds() {
    const size_t node_count = n * 2;
    parlay::random random(0);
    const size_t max_samples =
        1 + (node_count + parallel_block_size - 1) / parallel_block_size;
    sampled_id.resize(max_samples);
    size_t actual_samples = 0;

    auto sample = [&](size_t node_id) {
      if (link_list[node_id].get_sampled()) return;
      assert(actual_samples < max_samples);
      link_list[node_id].set_sampled(true);
      sampled_id[actual_samples++] = node_id;
    };

    sample(left_node_id(n - 1));
    for (size_t i = 1; i < max_samples; ++i) {
      sample(random.ith_rand(i) % node_count);
    }
    sampled_id.resize(actual_samples);
  }

  void ScanLinkListParallel() {
    BuildSampleIds();
    sampled_id_next_initial.resize(sampled_id.size());

    parlay::parallel_for(0, sampled_id.size(), [&](size_t i) {
      const size_t start_id = sampled_id[i];
      sampled_id_next_initial[i] = link_list[start_id].get_next();

      bool is_valid = link_list[start_id].get_valid();
      size_t node_id = link_list[start_id].get_next();
      while (node_id != kNullPtr) {
        is_valid = is_valid || link_list[node_id].get_valid();
        link_list[node_id].set_valid(is_valid);
        if (link_list[node_id].get_sampled()) break;
        node_id = link_list[node_id].get_next();
      }
      link(start_id, node_id);
    }, 1);

    size_t node_id = sampled_id[0];
    bool is_valid = false;
    while (node_id != kNullPtr) {
      is_valid = is_valid || link_list[node_id].get_valid();
      link_list[node_id].set_valid(is_valid);
      node_id = link_list[node_id].get_next();
    }

    parlay::parallel_for(0, sampled_id.size(), [&](size_t i) {
      const size_t start_id = sampled_id[i];
      bool segment_valid = link_list[start_id].get_valid();
      link(start_id, sampled_id_next_initial[i]);

      size_t segment_node_id = sampled_id_next_initial[i];
      while (segment_node_id != kNullPtr) {
        segment_valid =
            segment_valid || link_list[segment_node_id].get_valid();
        link_list[segment_node_id].set_valid(segment_valid);
        if (link_list[segment_node_id].get_sampled()) break;
        segment_node_id = link_list[segment_node_id].get_next();
      }
    }, 1);
  }

  void ScanLinkList() {
    ScanLinkListParallel();
  }

  void KernelParallel() {
    BuildFurthest();
    BuildLinkList();
    ScanLinkList();
    parlay::parallel_for(0, n, [&](size_t i) {
      valid[i] = left_node(i).get_valid() != right_node(i).get_valid();
    });
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
  parlay::sequence<LinkListNode> link_list;
  parlay::sequence<bool> valid;
  parlay::sequence<size_t> furthest_id;
  parlay::sequence<size_t> sampled_id;
  parlay::sequence<size_t> sampled_id_next_initial;
};

template <typename GetL, typename GetR>
parlay::sequence<bool> solve(size_t n, GetL left, GetR right) {
  Solver<GetL, GetR> solver(n, std::move(left), std::move(right));
  solver.Run();
  return std::move(solver.valid);
}

}  // namespace euler_tour
}  // namespace internal
}  // namespace interval_covering

#endif
