// Console test tool for the routing library. No window, no Vulkan.
//
//   routing_cli                      -> loads assets/zagreb.osm.pbf
//   routing_cli path/to/other.pbf    -> loads that instead

#include <core/paths.hpp>
#include <routing/graph.hpp>
#include <routing/parser.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point start) {
	return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void check_positions(const routing::Graph& g) {
	double minX = 0, maxX = 0, minY = 0, maxY = 0;
	for (const auto& p : g.position) {
		minX = std::min(minX, p.x);  maxX = std::max(maxX, p.x);
		minY = std::min(minY, p.y);  maxY = std::max(maxY, p.y);
	}

	std::size_t edges = 0, bad = 0;
	for (std::size_t u = 0; u < g.adj.size(); ++u) {
		for (const routing::Edge& e : g.adj[u]) {
			const auto& a = g.position[u];
			const auto& b = g.position[e.to_node];
			const double straight = std::hypot(b.x - a.x, b.y - a.y);
			++edges;
			if (straight > e.length * 1.01 + 1.0) ++bad;
		}
	}

	std::cout << "\norigin: " << g.origin.lat << ", " << g.origin.lon << "\n"
		<< "extent: " << (maxX - minX) / 1000.0 << " km east-west, "
		<< (maxY - minY) / 1000.0 << " km north-south\n"
		<< "position check: " << bad << " of " << edges
		<< " edges shorter than a straight line"
		<< (bad == 0 ? "  -- OK\n" : "  -- PROBLEM\n");
}

} // namespace

int main(int argc, char** argv) {
	try {
		std::string path;
		if (argc > 1) {
			path = argv[1];
		}
		else {
			paths::init(argv[0]);
			path = paths::asset("zagreb.osm.pbf").string();
		}
		std::cout << "Loading " << path << "\n";

		auto t0 = Clock::now();
		routing::Graph g = routing::build_graph(path);
		std::cout << "Parsed in " << ms_since(t0) << " ms\n\n";

		routing::print_stats(g);
		check_positions(g);
		std::cout << "\nreachable from 0: " << routing::CountReachable(g, 0) << "\n";

		t0 = Clock::now();
		auto dist = routing::dijkstra(g, 0);
		std::cout << "full Dijkstra from 0: " << ms_since(t0) << " ms\n";
		(void)dist;
	}
	catch (const std::exception& e) {
		std::cerr << "FATAL: " << e.what() << "\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}