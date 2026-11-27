#include "pta/solver.hpp"
#include <cassert>
#include <random>
#include <sstream>

static pta::Result run(const char *input) {
  std::istringstream in(input);
  auto w = pta::parse(in);
  auto r = pta::solve_reference(w);
  std::string error;
  assert(pta::verify_fixed_point(w, r, &error));
  return r;
}
int main() {
  // Address seeds, duplicate copy, cycle, and self edge.
  auto a = run("pta-v1\nvars 3\nobjects 1\nlocation 0 2\naddr 0 0\ncopy 0 "
               "1\ncopy 0 1\ncopy 1 0\ncopy 2 2\n");
  assert(a.points_to[0].count(0) && a.points_to[1].count(0));
  assert(a.points_to[2].empty());
  // Load adds location(object)->destination; store adds
  // source->location(object).
  auto b = run("pta-v1\nvars 5\nobjects 2\nlocation 0 2\nlocation 1 3\naddr 0 "
               "0\naddr 2 1\nload 0 4\nstore 0 1\n");
  assert(b.edges[2].count(4));
  assert(b.edges[1].count(2));
  assert(b.points_to[4].count(1));
  assert(b.points_to[2].count(1));
  // An indirect edge discovered after its source already has facts propagates
  // them.
  auto c = run("pta-v1\nvars 4\nobjects 2\nlocation 0 2\nlocation 1 3\naddr 0 "
               "0\naddr 2 1\ncopy 2 3\nload 0 3\n");
  assert(c.points_to[3].count(1));
  // Empty workload.
  auto d = run("pta-v1\nvars 0\nobjects 0\n");
  assert(d.points_to.empty());
  // Reproducible small-graph differential stress against the independent
  // declarative fixed-point checker. Fixed seed keeps failures reproducible.
  std::mt19937 rng(0x505441u);
  for (int trial = 0; trial < 200; ++trial) {
    const std::size_t nv = 1 + rng() % 8, no = 1 + rng() % 6;
    pta::Workload w;
    w.vars = nv;
    w.objects = no;
    w.locations.resize(no);
    for (auto &loc : w.locations)
      loc = rng() % nv;
    for (int i = 0, n = rng() % 10; i < n; ++i)
      w.addr.push_back({rng() % nv, rng() % no});
    auto generate = [&](std::vector<pta::Pair> &out, int count) {
      for (int i = 0; i < count; ++i)
        out.push_back({rng() % nv, rng() % nv});
    };
    generate(w.copy, rng() % 14);
    generate(w.load, rng() % 8);
    generate(w.store, rng() % 8);
    auto result = pta::solve_reference(w);
    std::string error;
    assert(pta::verify_fixed_point(w, result, &error));
  }
}
