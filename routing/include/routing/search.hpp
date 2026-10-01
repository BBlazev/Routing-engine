#pragma once

#include <routing/graph.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace routing {

enum class Algorithm { Dijkstra, AStar };

struct SearchResult {
	double distance = std::numeric_limits<double>::infinity();

	std::vector<uint32_t> pathVertices;  
	std::vector<uint32_t> pathSegments;  
	std::vector<uint32_t> settleOrder;   

	bool found() const { return !std::isinf(distance); }
};

SearchResult shortest_path(const Graph& g, uint32_t start, uint32_t target, Algorithm algorithm = Algorithm::Dijkstra);
} // namespace routing