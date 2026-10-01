#pragma once

#include <routing/geo.hpp>

#include <cstdint>
#include <vector>

namespace routing {

struct Segment {
	uint32_t from;
	uint32_t to;
	uint32_t firstPoint;
	uint32_t pointCount;
};

struct Edge {
	uint32_t to_node;
	double   length;    
	uint32_t segment;   
};

struct Graph {
	std::vector<std::vector<Edge>> adj;
	std::vector<Vec2d> position;  
	LatLon origin;    

	std::vector<Segment> segments; 
	std::vector<Vec2d> points;    
};

int  CountReachable(const Graph& g, int start);
void print_stats(const Graph& g);

std::vector<double> dijkstra(const Graph& g, uint32_t start);
std::vector<bool> strongly_connected_from(const Graph& g, uint32_t v);
uint32_t nearest_vertex(const Graph& g, Vec2d p, const std::vector<bool>* allowed = nullptr);
} // namespace routing