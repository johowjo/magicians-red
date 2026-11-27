#include "random_workload.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace pta::generator {
Workload random_workload(std::uint32_t seed, std::size_t vars,
                         std::size_t objects, std::size_t max_per_kind) {
  if (vars == 0 && objects != 0)
    throw std::invalid_argument("objects require at least one variable node");
  std::mt19937 rng(seed);
  auto count = [&]() -> std::size_t {
    return max_per_kind == 0 ? 0 : rng() % (max_per_kind + 1);
  };
  auto variable = [&]() -> Node { return rng() % vars; };
  auto object = [&]() -> Object { return rng() % objects; };

  Workload w;
  w.vars = vars;
  w.objects = objects;
  w.locations.resize(objects);
  for (auto &location : w.locations)
    location = variable();
  if (vars == 0)
    return w;
  if (objects != 0)
    for (std::size_t i = count(); i > 0; --i)
      w.addr.push_back({variable(), object()});
  auto add_edges = [&](std::vector<Pair> &out) {
    for (std::size_t i = count(); i > 0; --i)
      out.push_back({variable(), variable()});
  };
  add_edges(w.copy);
  add_edges(w.load);
  add_edges(w.store);
  return w;
}

std::string serialize(const Workload &w, std::uint32_t seed,
                      bool duplicate_constraints) {
  struct Record {
    std::string text;
    bool is_constraint;
  };
  std::vector<Record> records{{"vars " + std::to_string(w.vars), false},
                              {"objects " + std::to_string(w.objects), false}};
  for (std::size_t o = 0; o < w.objects; ++o)
    records.push_back(
        {"location " + std::to_string(o) + " " + std::to_string(w.locations[o]),
         false});
  auto append = [&](const char *name, const std::vector<Pair> &constraints) {
    for (auto p : constraints)
      records.push_back({std::string(name) + " " + std::to_string(p.first) +
                             " " + std::to_string(p.second),
                         true});
  };
  append("addr", w.addr);
  append("copy", w.copy);
  append("load", w.load);
  append("store", w.store);

  std::mt19937 rng(seed);
  if (duplicate_constraints) {
    const auto original_size = records.size();
    for (std::size_t i = 0; i < original_size; ++i)
      if (records[i].is_constraint && rng() % 4 == 0)
        records.push_back(records[i]);
  }
  std::shuffle(records.begin(), records.end(), rng);
  std::ostringstream out;
  out << "pta-v1\n";
  for (const auto &record : records)
    out << record.text << '\n';
  return out.str();
}
} // namespace pta::generator
