#include "pta/solver.hpp"
#include <sstream>
#include <stdexcept>

static void check(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

static pta::Result run(const char *input) {
  std::istringstream in(input);
  auto w = pta::parse(in);
  auto r = pta::solve_reference(w);
  std::string error;
  if (!pta::verify_fixed_point(w, r, &error))
    throw std::runtime_error("fixed-point verifier failed: " + error);
  return r;
}
int main() {
  // Address seeds, duplicate copy, cycle, and self edge.
  auto a = run("pta-v1\nvars 3\nobjects 1\nlocation 0 2\naddr 0 0\ncopy 0 "
               "1\ncopy 0 1\ncopy 1 0\ncopy 2 2\n");
  check(a.points_to[0].count(0) && a.points_to[1].count(0),
        "copy-cycle propagation failed");
  check(a.points_to[2].empty(), "unseeded self-copy should remain empty");
  // Load adds location(object)->destination; store adds
  // source->location(object).
  auto b = run("pta-v1\nvars 5\nobjects 2\nlocation 0 2\nlocation 1 3\naddr 0 "
               "0\naddr 2 1\nload 0 4\nstore 0 1\n");
  check(b.edges[2].count(4), "load inclusion edge missing");
  check(b.edges[1].count(2), "store inclusion edge missing");
  check(b.points_to[4].count(1), "load propagation failed");
  check(b.points_to[2].count(1), "store propagation failed");
  // An indirect edge discovered after its source already has facts propagates
  // them.
  auto c = run("pta-v1\nvars 4\nobjects 2\nlocation 0 2\nlocation 1 3\naddr 0 "
               "0\naddr 2 1\ncopy 2 3\nload 0 3\n");
  check(c.points_to[3].count(1), "late dynamic edge did not propagate");
  // Empty workload.
  auto d = run("pta-v1\nvars 0\nobjects 0\n");
  check(d.points_to.empty(), "zero-node workload should have zero result sets");
}
