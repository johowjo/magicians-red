#include "pta/constraints.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace pta {
namespace {
std::size_t number(const std::string &s, std::size_t line) {
  std::size_t used = 0;
  try {
    auto v = std::stoull(s, &used);
    if (used != s.size())
      throw std::invalid_argument("suffix");
    return static_cast<std::size_t>(v);
  } catch (...) {
    throw std::runtime_error("line " + std::to_string(line) +
                             ": invalid nonnegative integer '" + s + "'");
  }
}
} // namespace
Workload parse(std::istream &input) {
  Workload w;
  bool version = false, have_vars = false, have_objects = false;
  std::vector<std::pair<std::size_t, Node>> locations;
  std::string line;
  std::size_t ln = 0;
  while (std::getline(input, line)) {
    ++ln;
    auto hash = line.find('#');
    if (hash != std::string::npos)
      line.resize(hash);
    std::istringstream row(line);
    std::string op;
    if (!(row >> op))
      continue;
    if (!version) {
      if (op != "pta-v1")
        throw std::runtime_error("line " + std::to_string(ln) +
                                 ": expected pta-v1");
      version = true;
      continue;
    }
    std::vector<std::string> args;
    std::string a;
    while (row >> a)
      args.push_back(a);
    auto arity = [&](std::size_t n) {
      if (args.size() != n)
        throw std::runtime_error("line " + std::to_string(ln) +
                                 ": wrong number of fields for " + op);
    };
    if (op == "vars" || op == "objects") {
      arity(1);
      auto n = number(args[0], ln);
      bool &seen = op == "vars" ? have_vars : have_objects;
      if (seen)
        throw std::runtime_error("line " + std::to_string(ln) + ": duplicate " +
                                 op);
      seen = true;
      (op == "vars" ? w.vars : w.objects) = n;
    } else if (op == "location") {
      arity(2);
      locations.push_back({number(args[0], ln), number(args[1], ln)});
    } else {
      arity(2);
      Pair p{number(args[0], ln), number(args[1], ln)};
      if (op == "addr")
        w.addr.push_back(p);
      else if (op == "copy")
        w.copy.push_back(p);
      else if (op == "load")
        w.load.push_back(p);
      else if (op == "store")
        w.store.push_back(p);
      else
        throw std::runtime_error("line " + std::to_string(ln) +
                                 ": unknown record " + op);
    }
  }
  if (!version || !have_vars || !have_objects)
    throw std::runtime_error("missing version, vars, or objects declaration");
  w.locations.assign(w.objects, w.vars);
  std::vector<bool> mapped(w.objects, false);
  for (auto [o, n] : locations) {
    if (o >= w.objects || n >= w.vars)
      throw std::runtime_error("location ID out of range");
    if (mapped[o])
      throw std::runtime_error("duplicate location mapping for object " +
                               std::to_string(o));
    mapped[o] = true;
    w.locations[o] = n;
  }
  for (std::size_t o = 0; o < w.objects; ++o)
    if (!mapped[o])
      throw std::runtime_error("missing location mapping for object " +
                               std::to_string(o));
  auto check = [&](const std::vector<Pair> &ps, bool first_obj, bool second_obj,
                   const char *name) {
    for (auto p : ps)
      if ((first_obj ? p.first >= w.objects : p.first >= w.vars) ||
          (second_obj ? p.second >= w.objects : p.second >= w.vars))
        throw std::runtime_error(std::string(name) +
                                 " constraint ID out of range");
  };
  check(w.addr, false, true, "addr");
  check(w.copy, false, false, "copy");
  check(w.load, false, false, "load");
  check(w.store, false, false, "store");
  return w;
}
Workload parse_file(const std::string &path) {
  std::ifstream in(path);
  if (!in)
    throw std::runtime_error("cannot open " + path);
  return parse(in);
}
} // namespace pta
