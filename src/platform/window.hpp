#ifndef PLATFORM_WINDOW_HPP
#define PLATFORM_WINDOW_HPP

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <functional>

class Window {
public:

	Window(int width, int height, const char* title);
	~Window();

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	Window(Window&&) = delete;
	Window& operator=(Window&&) = delete;

	[[nodiscard]] GLFWwindow* handle() const { return window_; }
	[[nodiscard]] bool should_close() const;
	void request_close();

	void poll_events() const;
	void wait_events() const;

	[[nodiscard]] VkExtent2D framebuffer_extent() const;
	[[nodiscard]] bool is_minimised() const;

	[[nodiscard]] bool key_pressed(int glfwKey) const;
	[[nodiscard]] bool mouse_button_pressed(int glfwButton) const;

	void set_resize_callback(std::function<void(int, int)> callback) {
		onResize_ = std::move(callback);
	}

private:
	static void framebuffer_size_callback(GLFWwindow* window, int width, int height);

	GLFWwindow* window_ = nullptr;
	std::function<void(int, int)>  onResize_;
};

#endif // PLATFORM_WINDOW_HPP