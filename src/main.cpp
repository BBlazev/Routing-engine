#include <core/paths.hpp>
#include <core/settings.hpp>
#include <platform/window.hpp>
#include <renderer/vk_renderer.hpp>
#include <vulkan/vk_device.hpp>
#include <routing/parser.hpp>
#include <routing/search.hpp>
#include <city/road_mesh.hpp>

#include <imgui.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
	(void)argc;

	try {
		paths::init(argv[0]);

		std::cout << "Project root: " << paths::root().string() << "\n";

		routing::Graph     roads;
		city::LineMeshData roadMesh;
		std::vector<bool>  mainNetwork;

		const auto osmPath = paths::asset("zagreb.osm.pbf");
		if (std::filesystem::exists(osmPath)) {
			roads = routing::build_graph(osmPath.string());
			roadMesh = city::build_road_mesh(roads);
			std::cout << "Road mesh: " << roadMesh.indices.size() / 2 << " lines\n";

			mainNetwork = routing::strongly_connected_from(roads, routing::nearest_vertex(roads, { 0.0, 0.0 }));
			std::cout << "Main network: "
				<< std::count(mainNetwork.begin(), mainNetwork.end(), true)
				<< " of " << roads.adj.size() << " intersections\n";
		}
		else {
			std::cout << "No OSM file at " << osmPath.string() << " -- routing skipped\n";
		}

		Window window{ settings::WINDOW_WIDTH, settings::WINDOW_HEIGHT, settings::WINDOW_TITLE };

		VulkanDevice device{ window.handle() };
		Renderer     renderer{ window, device };

		constexpr uint32_t kRoadLayer = 0;
		constexpr uint32_t kRouteLayer = 1;
		constexpr uint32_t kMarkerLayer = 2;

		const glm::vec4 kStartColor{ 0.2f, 1.0f, 0.3f, 1.0f };
		const glm::vec4 kEndColor{ 1.0f, 0.2f, 0.2f, 1.0f };

		if (!roadMesh.indices.empty()) {
			renderer.set_lines(kRoadLayer, roadMesh.indices, roadMesh.vertices);

			renderer.camera().position = { 0.0f, 20000.0f, 0.0f };
			renderer.camera().pitch = -1.55f;
			renderer.camera().moveSpeed = 2000.0f;
		}

		std::cout << "Ready. ESC or close the window to exit.\n";

		double lastTime = glfwGetTime();

		bool     leftWasDown = false;
		bool     haveStart = false;
		uint32_t startVertex = 0;

		while (!window.should_close()) {
			window.poll_events();

			const double now = glfwGetTime();
			const float dt = static_cast<float>(std::min(now - lastTime, 0.1));
			lastTime = now;

			if (window.key_pressed(GLFW_KEY_ESCAPE)) {
				window.request_close();
			}

			const bool leftDown = window.mouse_button_pressed(GLFW_MOUSE_BUTTON_LEFT);
			const bool clicked = leftDown && !leftWasDown && !ImGui::GetIO().WantCaptureMouse;
			leftWasDown = leftDown;

			if (clicked && !roads.adj.empty()) {
				double mx = 0.0, my = 0.0;
				glfwGetCursorPos(window.handle(), &mx, &my);

				if (auto hit = renderer.screen_to_ground(mx, my)) {
					const uint32_t v = routing::nearest_vertex(roads, city::to_map(*hit), &mainNetwork);
					const float markerSize = std::max(20.0f, renderer.camera().position.y * 0.02f);

					if (!haveStart) {
						startVertex = v;
						haveStart = true;

						renderer.set_lines(kRouteLayer, {}, {});

						city::LineMeshData markers;
						city::append_marker(markers, roads.position[v], markerSize, kStartColor);
						renderer.set_lines(kMarkerLayer, markers.indices, markers.vertices);

						std::cout << "Start: vertex " << v << "\n";
					}
					else {
						haveStart = false;

						const routing::SearchResult route =
							routing::shortest_path(roads, startVertex, v, routing::Algorithm::AStar);

						const city::LineMeshData routeMesh = city::build_route_mesh(roads, route);
						renderer.set_lines(kRouteLayer, routeMesh.indices, routeMesh.vertices);

						city::LineMeshData markers;
						city::append_marker(markers, roads.position[startVertex], markerSize, kStartColor);
						city::append_marker(markers, roads.position[v], markerSize, kEndColor);
						renderer.set_lines(kMarkerLayer, markers.indices, markers.vertices);

						if (route.found())
							std::cout << "Route: " << route.distance / 1000.0 << " km, "
							<< route.settleOrder.size() << " vertices settled\n";
						else
							std::cout << "Route: none\n";
					}
				}
			}

			renderer.update(dt);
			renderer.draw();
		}
	}
	catch (const std::exception& e) {
		std::cerr << "FATAL: " << e.what() << "\n";
		return EXIT_FAILURE;
	}

	std::cout << "Clean shutdown.\n";
	return EXIT_SUCCESS;
}