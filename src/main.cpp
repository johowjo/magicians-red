#include "pta/solver.hpp"
#include <iostream>
int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: pta_ref workload.pta\n";
    return 2;
  }
  try {
    auto w = pta::parse_file(argv[1]);
    auto r = pta::solve_reference(w);
    std::string error;
    if (!pta::verify_fixed_point(w, r, &error)) {
      std::cerr << "verification failed: " << error << '\n';
      return 1;
    }
    for (std::size_t v = 0; v < r.points_to.size(); ++v) {
      std::cout << "pt " << v;
      for (auto o : r.points_to[v])
        std::cout << ' ' << o;
      std::cout << '\n';
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
