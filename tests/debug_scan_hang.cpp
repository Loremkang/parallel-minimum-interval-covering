#include "interval_covering_euler.h"
#include <iostream>
#include <vector>

int main() {
  std::cout << "Testing ScanLinkListParallel with debug output (Euler tour implementation)\n";

  const size_t n = 1000;
  std::vector<std::pair<int, int>> intervals;

  // Generate test data
  int left = 0;
  int right = 10;
  for (size_t i = 0; i < n; i++) {
    intervals.push_back({left, right});
    left += (rand() % 5) + 1;
    right += (rand() % 5) + 4;
  }

  auto getL = [&](size_t i) { return intervals[i].first; };
  auto getR = [&](size_t i) { return intervals[i].second; };

  IntervalCoveringEuler solver(intervals.size(), getL, getR);
  solver.valid = parlay::sequence<bool>(n, 0);

  std::cout << "Step 1: BuildFurthest\n";
  solver.BuildFurthest();

  std::cout << "Step 2: BuildLinkList\n";
  solver.BuildLinkList();

  std::cout << "Step 3: BuildSampleId\n";
  solver.BuildSampleId();

  std::cout << "Number of sampled nodes: " << solver.sampled_id.size() << "\n";
  for (size_t i = 0; i < solver.sampled_id.size(); i++) {
    std::cout << "  sampled_id[" << i << "] = " << solver.sampled_id[i] << "\n";
  }

  std::cout << "\nStep 4: First parallel scan (building sampled sketch)\n";
  std::cout << "Press Ctrl+C if this hangs...\n";
  std::cout.flush();

  // Manually inline the first parlay::parallel_for
  solver.sampled_id_nxt_initial.resize(solver.sampled_id.size());
  parlay::parallel_for(0, solver.sampled_id.size(), [&](size_t i) {
    std::cout << "Thread " << i << " starting from node " << solver.sampled_id[i] << "\n";
    std::cout.flush();

    solver.sampled_id_nxt_initial[i] = solver.link_list[solver.sampled_id[i]].get_nxt();

    size_t start_id = solver.sampled_id[i];
    bool valid = solver.link_list[start_id].get_valid();
    size_t node_id = solver.link_list[start_id].get_nxt();

    size_t steps = 0;
    while (node_id != solver.kNullPtr) {
      steps++;
      if (steps > n * 3) {
        std::cout << "Thread " << i << " seems stuck! Steps: " << steps
                  << ", current node: " << node_id << "\n";
        std::cout.flush();
        break;
      }

      valid = valid || solver.link_list[node_id].get_valid();
      solver.link_list[node_id].set_valid(valid);

      if (solver.link_list[node_id].get_sampled()) {
        std::cout << "Thread " << i << " found next sampled node " << node_id
                  << " after " << steps << " steps\n";
        std::cout.flush();
        break;
      }
      node_id = solver.link_list[node_id].get_nxt();
    }

    solver.link(solver.sampled_id[i], node_id);
    std::cout << "Thread " << i << " done\n";
    std::cout.flush();
  }, 1);

  std::cout << "\nStep 5: Serial scan over sampled sketch\n";
  std::cout << "This is where it might hang...\n";
  std::cout.flush();

  size_t node_id = solver.sampled_id[0];
  bool valid = false;
  size_t steps = 0;
  while (node_id != solver.kNullPtr) {
    steps++;
    std::cout << "Step " << steps << ": node " << node_id << "\n";
    std::cout.flush();

    if (steps > 100) {
      std::cout << "HUNG! Stuck in infinite loop\n";
      std::cout << "Current node: " << node_id << "\n";
      std::cout << "Next node: " << solver.link_list[node_id].get_nxt() << "\n";
      break;
    }

    valid = valid || solver.link_list[node_id].get_valid();
    solver.link_list[node_id].set_valid(valid);
    node_id = solver.link_list[node_id].get_nxt();
  }

  std::cout << "\nStep 6: Second parallel scan (restoring and scanning)\n";
  std::cout << "This might also hang...\n";
  std::cout.flush();

  parlay::parallel_for(0, solver.sampled_id.size(), [&](size_t i) {
    std::cout << "Thread " << i << " restoring link and scanning\n";
    std::cout.flush();

    // Restore the original link list
    solver.link(solver.sampled_id[i], solver.sampled_id_nxt_initial[i]);

    size_t start_id = solver.sampled_id[i];
    bool valid = solver.link_list[start_id].get_valid();
    size_t node_id = solver.link_list[start_id].get_nxt();

    size_t steps = 0;
    while (node_id != solver.kNullPtr) {
      steps++;
      if (steps > n * 3) {
        std::cout << "Thread " << i << " stuck in step 6! Steps: " << steps
                  << ", node: " << node_id << "\n";
        std::cout.flush();
        break;
      }

      valid = valid || solver.link_list[node_id].get_valid();
      solver.link_list[node_id].set_valid(valid);

      if (solver.link_list[node_id].get_sampled()) {
        std::cout << "Thread " << i << " reached next sampled node after "
                  << steps << " steps\n";
        std::cout.flush();
        break;
      }
      node_id = solver.link_list[node_id].get_nxt();
    }

    std::cout << "Thread " << i << " done with step 6\n";
    std::cout.flush();
  }, 1);

  std::cout << "\nCompleted all steps without hanging!\n";

  return 0;
}
