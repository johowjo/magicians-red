#include "random_workload.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
std::size_t parse_size(const char *text, const char *name) {
  std::size_t used = 0;
  auto value = std::stoull(text, &used);
  if (text[used] != '\0')
    throw std::invalid_argument(std::string("invalid ") + name);
  return value;
}
} // namespace

int main(int argc, char **argv) {
  if (argc < 5 || argc > 6) {
    std::cerr << "usage: pta_gen OUTPUT.pta SEED VARS OBJECTS "
                 "[MAX_CONSTRAINTS_PER_KIND]\n";
    return 2;
  }
  try {
    auto seed = static_cast<std::uint32_t>(parse_size(argv[2], "seed"));
    auto vars = parse_size(argv[3], "variable count");
    auto objects = parse_size(argv[4], "object count");
    auto max_constraints =
        argc == 6 ? parse_size(argv[5], "constraint limit") : 8;
    auto workload =
        pta::generator::random_workload(seed, vars, objects, max_constraints);
    std::ofstream output(argv[1]);
    if (!output)
      throw std::runtime_error(std::string("cannot open output file ") +
                               argv[1]);
    output << pta::generator::serialize(workload, seed);
    if (!output)
      throw std::runtime_error("failed writing workload");
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
