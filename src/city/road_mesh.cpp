#include "road_mesh.hpp"

namespace city {

namespace {


glm::vec3 to_world(routing::Vec2d p) {
	return { static_cast<float>(p.x), 0.0f, static_cast<float>(-p.y) };
}

} // namespace

LineMeshData build_road_mesh(const routing::Graph& g) {
	LineMeshData mesh;


	mesh.vertices.reserve(g.points.size());
	for (const routing::Vec2d& p : g.points) {
		Vertex v{};
		v.position = to_world(p);
		v.normal = { 0.0f, 1.0f, 0.0f };
		v.color = { 0.85f, 0.85f, 0.85f, 1.0f };
		mesh.vertices.push_back(v);
	}

	for (const routing::Segment& s : g.segments) {
		for (uint32_t i = 1; i < s.pointCount; ++i) {
			mesh.indices.push_back(s.firstPoint + i - 1);
			mesh.indices.push_back(s.firstPoint + i);
		}
	}

	return mesh;
}

LineMeshData build_route_mesh(const routing::Graph& g, const routing::SearchResult& route) {

	constexpr float kRouteLift = 2.0f; 
	LineMeshData mesh;

	for (uint32_t segId : route.pathSegments) {
		const routing::Segment& s = g.segments[segId];
		const auto base = static_cast<uint32_t>(mesh.vertices.size());

		for (uint32_t i = 0; i < s.pointCount; ++i) {
			Vertex v{};
			v.position = to_world(g.points[s.firstPoint + i]);
			v.position.y = kRouteLift;
			v.normal = { 0.0f, 1.0f, 0.0f };
			v.color = { 1.0f, 0.55f, 0.1f, 1.0f };
			mesh.vertices.push_back(v);
		}

		for (uint32_t i = 1; i < s.pointCount; ++i) {
			mesh.indices.push_back(base + i - 1);
			mesh.indices.push_back(base + i);
		}
	}

	return mesh;
}

routing::Vec2d to_map(glm::vec3 world) {
	return { static_cast<double>(world.x), static_cast<double>(-world.z) };
}

void append_marker(LineMeshData& mesh, routing::Vec2d at, float size, glm::vec4 color) {
	constexpr float kMarkerLift = 4.0f;   

	const glm::vec3 c = to_world(at) + glm::vec3(0.0f, kMarkerLift, 0.0f);
	const float h = size * 0.5f;

	const glm::vec3 corners[4] = {
		c + glm::vec3(-h, 0.0f, -h),
		c + glm::vec3(h, 0.0f, -h),
		c + glm::vec3(h, 0.0f,  h),
		c + glm::vec3(-h, 0.0f,  h),
	};

	const auto base = static_cast<uint32_t>(mesh.vertices.size());
	for (const glm::vec3& p : corners) {
		Vertex v{};
		v.position = p;
		v.normal = { 0.0f, 1.0f, 0.0f };
		v.color = color;
		mesh.vertices.push_back(v);
	}

	const uint32_t lines[] = { 0,1,  1,2,  2,3,  3,0,  0,2,  1,3 };
	for (uint32_t i : lines)
		mesh.indices.push_back(base + i);
}

} // namespace city