#include <core/paths.hpp>
#include <routing/graph.hpp>
#include <routing/parser.hpp>
#include <routing/search.hpp>

#include <random>
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
struct TestPair {
	uint32_t start;
	uint32_t target;
	double   expected;  
};

std::vector<TestPair> make_test_pairs(const routing::Graph& g, int count) {
	std::mt19937 rng{ 12345 };
	std::uniform_int_distribution<uint32_t> pick(0, static_cast<uint32_t>(g.adj.size() - 1));

	std::vector<TestPair> pairs;
	for (int i = 0; i < count; ++i) {
		const uint32_t s = pick(rng);
		const uint32_t t = pick(rng);
		pairs.push_back({ s, t, routing::dijkstra(g, s)[t] });
	}
	return pairs;
}

void check_search(const routing::Graph& g, const std::vector<TestPair>& pairs,
	routing::Algorithm algorithm, const char* name) {
	int reachable = 0, wrongDistance = 0, brokenPath = 0;
	std::size_t settledTotal = 0;
	double totalMs = 0.0;

	for (const TestPair& p : pairs) {
		auto t0 = Clock::now();
		const routing::SearchResult r = routing::shortest_path(g, p.start, p.target, algorithm);
		totalMs += ms_since(t0);

		if (!r.found()) {
			if (!std::isinf(p.expected)) ++wrongDistance;
			continue;
		}
		++reachable;
		settledTotal += r.settleOrder.size();

		if (std::abs(r.distance - p.expected) > 1e-6) ++wrongDistance;

		bool ok = r.pathVertices.front() == p.start
			&& r.pathVertices.back() == p.target
			&& r.pathSegments.size() + 1 == r.pathVertices.size();

		double sum = 0.0;
		for (std::size_t k = 0; ok && k < r.pathSegments.size(); ++k) {
			const uint32_t a = r.pathVertices[k];
			const uint32_t b = r.pathVertices[k + 1];

			const routing::Edge* edge = nullptr;
			for (const routing::Edge& e : g.adj[a])
				if (e.to_node == b && e.segment == r.pathSegments[k]) { edge = &e; break; }

			if (edge) sum += edge->length;
			else      ok = false;
		}
		if (!ok || std::abs(sum - r.distance) > 1e-6) ++brokenPath;
	}

	const std::size_t avgSettled = settledTotal / static_cast<std::size_t>(std::max(reachable, 1));
	const bool pass = wrongDistance == 0 && brokenPath == 0;

	std::cout << "\n" << name << ": " << reachable << " reachable, "
		<< "wrong distance: " << wrongDistance << ", broken path: " << brokenPath
		<< (pass ? "  -- OK\n" : "  -- PROBLEM\n")
		<< "  avg settled: " << avgSettled << " ("
		<< 100.0 * static_cast<double>(avgSettled) / static_cast<double>(g.adj.size())
		<< "%), avg query: " << totalMs / static_cast<double>(pairs.size()) << " ms\n";
}

void check_segments(const routing::Graph& g) {
	auto dist = [](routing::Vec2d a, routing::Vec2d b) { return std::hypot(b.x - a.x, b.y - a.y); };

	std::size_t badEnds = 0;
	for (const routing::Segment& s : g.segments) {
		const auto& first = g.points[s.firstPoint];
		const auto& last = g.points[s.firstPoint + s.pointCount - 1];
		if (dist(first, g.position[s.from]) > 0.01 || dist(last, g.position[s.to]) > 0.01) ++badEnds;
	}

	std::size_t badLength = 0;
	for (std::size_t u = 0; u < g.adj.size(); ++u) {
		for (const routing::Edge& e : g.adj[u]) {
			const routing::Segment& s = g.segments[e.segment];
			double polyline = 0.0;
			for (uint32_t i = 1; i < s.pointCount; ++i)
				polyline += dist(g.points[s.firstPoint + i - 1], g.points[s.firstPoint + i]);
			if (std::abs(polyline - e.length) > e.length * 0.01 + 1.0) ++badLength;
		}
	}

	std::cout << "\nsegments: " << g.segments.size() << ", points: " << g.points.size()
		<< " (avg " << static_cast<double>(g.points.size()) / static_cast<double>(g.segments.size())
		<< " per segment)\n"
		<< "segment ends check: " << badEnds << " bad" << (badEnds == 0 ? "  -- OK\n" : "  -- PROBLEM\n")
		<< "segment length check: " << badLength << " bad" << (badLength == 0 ? "  -- OK\n" : "  -- PROBLEM\n");
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

		check_segments(g);
		const std::vector<TestPair> pairs = make_test_pairs(g, 100);
		check_search(g, pairs, routing::Algorithm::Dijkstra, "Dijkstra");
		check_search(g, pairs, routing::Algorithm::AStar, "A*");
	}
	catch (const std::exception& e) {
		std::cerr << "FATAL: " << e.what() << "\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}