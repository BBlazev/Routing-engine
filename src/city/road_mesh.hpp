#pragma once

#include <routing/graph.hpp>
#include <vulkan/vk_types.hpp>
#include <routing/search.hpp>
#include <cstdint>
#include <vector>

namespace city {


struct LineMeshData {
	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;
};

LineMeshData build_road_mesh(const routing::Graph& g);
LineMeshData build_route_mesh(const routing::Graph& g, const routing::SearchResult& route);
routing::Vec2d to_map(glm::vec3 world);
void append_marker(LineMeshData& mesh, routing::Vec2d at, float size, glm::vec4 color);
} // namespace city