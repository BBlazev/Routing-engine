#include <core/paths.hpp>
#include <cstdlib>
#include <routing/graph.hpp>
#include <routing/parser.hpp>

#include <chrono>
#include <iostream>

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start)
      .count();
}

} // namespace

int main(int argc, char **argv) {
  try {
    std::string path;
    if (argc > 1) {
      path = argv[1];
    } else {
      paths::init(argv[0]);
      path = paths::asset("zagreb.osm.pbf").string();
    }
    std::cout << "Loading " << path << "\n";

    auto t0 = Clock::now();
    routing::Graph g = routing::build_graph(path);
    std::cout << "Parsed in " << ms_since(t0) << " ms\n\n";

    routing::print_stats(g);
    std::cout << "\nreachable from 0: " << routing::CountReachable(g, 0)
              << "\n";

    t0 = Clock::now();
    auto dist = routing::dijkstra(g, 0);
    std::cout << "full Dijkstra from 0: " << ms_since(t0) << " ms\n";
    (void)dist;
  } catch (const std::exception &e) {
    std::cerr << "FATAL: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
