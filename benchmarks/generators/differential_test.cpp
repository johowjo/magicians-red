#include "pta/solver.hpp"
#include "random_workload.hpp"
#include <cstdint>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace {
pta::Result solve_text(const std::string &text, const std::string &label) {
  std::istringstream input(text);
  auto workload = pta::parse(input);
  auto result = pta::solve_reference(workload);
  std::string error;
  if (!pta::verify_fixed_point(workload, result, &error))
    throw std::runtime_error(label + ": verifier rejected result: " + error);
  return result;
}
} // namespace

int main() {
  constexpr std::uint32_t base_seed = 0x505441u;
  constexpr std::size_t trials = 300;
  std::mt19937 rng(base_seed);
  try {
    for (std::size_t trial = 0; trial < trials; ++trial) {
      const auto seed = static_cast<std::uint32_t>(rng());
      const std::size_t vars = 1 + rng() % 8;
      const std::size_t objects = rng() % 7;
      const std::size_t max_constraints = rng() % 12;
      auto workload =
          pta::generator::random_workload(seed, vars, objects, max_constraints);

      const auto original_text = pta::generator::serialize(workload, seed);
      const auto original =
          solve_text(original_text, "trial " + std::to_string(trial) +
                                        " seed " + std::to_string(seed));

      // Reordering every record and duplicating a subset of constraints must
      // preserve the exact points-to result and induced edge set.
      const auto variant_text =
          pta::generator::serialize(workload, seed ^ 0x9e3779b9u, true);
      const auto variant = solve_text(
          variant_text, "metamorphic trial " + std::to_string(trial) +
                            " seed " + std::to_string(seed));
      if (original.points_to != variant.points_to ||
          original.edges != variant.edges)
        throw std::runtime_error("reordering/duplicating constraints changed "
                                 "the result at trial " +
                                 std::to_string(trial) + " seed " +
                                 std::to_string(seed));
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  std::cout << "passed " << trials << " seeded differential/metamorphic cases"
            << " (base seed " << base_seed << ")\n";
}
