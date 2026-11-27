#include "pta/solver.hpp"
#include <stdexcept>

namespace pta {
Result solve_reference(const Workload &w) {
  Result r{std::vector<std::set<Object>>(w.vars),
           std::vector<std::set<Node>>(w.vars)};
  for (auto p : w.addr)
    r.points_to[p.first].insert(p.second);
  for (auto p : w.copy)
    r.edges[p.first].insert(p.second);
  bool changed;
  do {
    changed = false;
    // Discover all currently justified dynamic inclusions.
    for (auto p : w.load)
      for (auto o : r.points_to[p.first])
        changed |= r.edges[w.locations[o]].insert(p.second).second;
    for (auto p : w.store)
      for (auto o : r.points_to[p.first])
        changed |= r.edges[p.second].insert(w.locations[o]).second;
    // Propagate until ordinary edges are saturated. Any newly discovered edge
    // is picked up on this pass and then all of its source's existing facts
    // flow.
    bool propagated;
    do {
      propagated = false;
      for (Node src = 0; src < w.vars; ++src)
        for (Node dst : r.edges[src])
          for (Object o : r.points_to[src])
            propagated |= r.points_to[dst].insert(o).second;
      changed |= propagated;
    } while (propagated);
  } while (changed);
  return r;
}

bool verify_fixed_point(const Workload &w, const Result &r,
                        std::string *error) {
  auto fail = [&](const std::string &s) {
    if (error)
      *error = s;
    return false;
  };
  if (r.points_to.size() != w.vars || r.edges.size() != w.vars ||
      w.locations.size() != w.objects)
    return fail("result/workload dimensions disagree");
  for (auto p : w.addr)
    if (!r.points_to[p.first].count(p.second))
      return fail("address seed missing");
  std::vector<std::set<Node>> required(w.vars);
  for (auto p : w.copy)
    required[p.first].insert(p.second);
  for (auto p : w.load)
    for (auto o : r.points_to[p.first])
      required[w.locations[o]].insert(p.second);
  for (auto p : w.store)
    for (auto o : r.points_to[p.first])
      required[p.second].insert(w.locations[o]);
  for (Node src = 0; src < w.vars; ++src) {
    for (Node dst : required[src])
      if (!r.edges[src].count(dst))
        return fail("required inclusion edge missing");
    for (Node dst : r.edges[src]) {
      if (dst >= w.vars)
        return fail("edge destination out of range");
      for (Object o : r.points_to[src])
        if (!r.points_to[dst].count(o))
          return fail("inclusion edge is not satisfied");
    }
  }
  // Independent closure check: repeatedly apply the declarative rules without
  // consulting the solver's edge graph or its propagation routine.
  std::vector<std::set<Object>> closure(w.vars);
  for (auto p : w.addr)
    closure[p.first].insert(p.second);
  bool changed;
  do {
    changed = false;
    for (auto p : w.copy)
      for (auto o : closure[p.first])
        changed |= closure[p.second].insert(o).second;
    for (auto p : w.load)
      for (auto o : closure[p.first])
        for (auto x : closure[w.locations[o]])
          changed |= closure[p.second].insert(x).second;
    for (auto p : w.store)
      for (auto o : closure[p.first])
        for (auto x : closure[p.second])
          changed |= closure[w.locations[o]].insert(x).second;
  } while (changed);
  if (closure != r.points_to)
    return fail("points-to result differs from independently computed least "
                "fixed point");
  return true;
}
} // namespace pta
