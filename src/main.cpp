#include <core/paths.hpp>
#include <core/settings.hpp>
#include <platform/window.hpp>
#include <renderer/vk_renderer.hpp>
#include <vulkan/vk_device.hpp>
#include <routing/parser.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
	(void)argc;

	try {
		paths::init(argv[0]);

		std::cout << "Project root: " << paths::root().string() << "\n";

		// Smoke test: proves the routing library links into the engine and the
		// data loads. Temporary -- Phase 1 replaces this with real map loading.
		{
			const auto osmPath = paths::asset("zagreb.osm.pbf");
			if (std::filesystem::exists(osmPath)) {
				routing::Graph roads = routing::build_graph(osmPath.string());
				std::cout << "Road graph: " << roads.adj.size() << " vertices\n";
			}
			else {
				std::cout << "No OSM file at " << osmPath.string() << " -- routing skipped\n";
			}
		}

		Window window{ settings::WINDOW_WIDTH, settings::WINDOW_HEIGHT, settings::WINDOW_TITLE };

		VulkanDevice device{ window.handle() };
		Renderer     renderer{ window, device };

		std::cout << "Ready. ESC or close the window to exit.\n";

		double lastTime = glfwGetTime();

		while (!window.should_close()) {
			window.poll_events();

			const double now = glfwGetTime();
			const float dt = static_cast<float>(std::min(now - lastTime, 0.1));
			lastTime = now;

			if (window.key_pressed(GLFW_KEY_ESCAPE)) {
				window.request_close();
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