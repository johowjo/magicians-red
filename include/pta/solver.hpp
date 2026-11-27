#pragma once

#include "pta/constraints.hpp"
#include <set>
#include <vector>

namespace pta {
struct Result {
  std::vector<std::set<Object>> points_to;
  std::vector<std::set<Node>> edges;
};

// Deliberately simple deterministic CPU correctness oracle.
Result solve_reference(const Workload &workload);
// Re-derives indirect edges from the result and checks every rule
// independently.
bool verify_fixed_point(const Workload &workload, const Result &result,
                        std::string *error = nullptr);
} // namespace pta
