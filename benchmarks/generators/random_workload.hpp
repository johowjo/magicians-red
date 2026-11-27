#pragma once

#include "pta/constraints.hpp"
#include <cstdint>
#include <string>

namespace pta::generator {
// Generate a deterministic small graph. Each constraint family receives a
// random count in [0, max_per_kind].
Workload random_workload(std::uint32_t seed, std::size_t vars,
                         std::size_t objects, std::size_t max_per_kind);

// Serialize valid pta-v1 input. Records are shuffled; when requested, some
// constraints are duplicated to support metamorphic tests.
std::string serialize(const Workload &workload, std::uint32_t seed,
                      bool duplicate_constraints = false);
} // namespace pta::generator
