#pragma once

#include <cstdint>
#include <vector>

namespace routing {

struct Edge {
    uint32_t to_node;
    double   length;   // metres
};

struct Graph {
    std::vector<std::vector<Edge>> adj;
};

int  CountReachable(const Graph& g, int start);
void print_stats(const Graph& g);

std::vector<double> dijkstra(const Graph& g, uint32_t start);

} // namespace routing
