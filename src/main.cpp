#include <cstdlib>
#include <iostream>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/window.hpp>
#include <vulkan/vulkan_context.hpp>


int main() {
    try {
        Window window;
        std::cout << "Window created: " << window.get_width() << "x" << window.get_height()<< "\n";

        VulkanContext vk{window.get_window()};
        std::cout << "Vulkan initialized.\n\n";
        std::cout << "Setup complete. Close the window or press ESC to exit.\n";

        while (!window.should_close()) {
            glfwPollEvents();

            if (glfwGetKey(window.get_window(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window.get_window(), GLFW_TRUE);
            }

            vk.draw();
            
        }

    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "Clean shutdown.\n";
    return EXIT_SUCCESS;
}
