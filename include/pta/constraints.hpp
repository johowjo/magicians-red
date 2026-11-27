#pragma once

#include <cstddef>
#include <istream>
#include <string>
#include <vector>

namespace pta {
using Node = std::size_t;
using Object = std::size_t;

struct Pair {
  std::size_t first, second;
};
struct Workload {
  std::size_t vars = 0, objects = 0;
  std::vector<Node> locations;
  std::vector<Pair> addr, copy, load, store;
};

Workload parse(std::istream &input);
Workload parse_file(const std::string &path);
} // namespace pta
