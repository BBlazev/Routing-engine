#include <platform/window.hpp>

#include <stdexcept>

Window::Window(int width, int height, const char* title) {

	if (!glfwInit()) {
		throw std::runtime_error("Failed to initialise GLFW");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
	if (window_ == nullptr) {
		glfwTerminate();
		throw std::runtime_error("Failed to create window");
	}

	glfwSetWindowUserPointer(window_, this);
	glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
}

Window::~Window() {
	if (window_ != nullptr) glfwDestroyWindow(window_);
	glfwTerminate();
}

void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
	if (self != nullptr && self->onResize_) {
		self->onResize_(width, height);
	}
}

bool Window::should_close() const {
	return glfwWindowShouldClose(window_) != 0;
}

void Window::request_close() {
	glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

void Window::poll_events() const {
	glfwPollEvents();
}

void Window::wait_events() const {
	glfwWaitEvents();
}

VkExtent2D Window::framebuffer_extent() const {
	int w = 0;
	int h = 0;
	glfwGetFramebufferSize(window_, &w, &h);
	return VkExtent2D{ static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
}

bool Window::is_minimised() const {
	const VkExtent2D e = framebuffer_extent();
	return e.width == 0 || e.height == 0;
}

bool Window::key_pressed(int glfwKey) const {
	return glfwGetKey(window_, glfwKey) == GLFW_PRESS;
}

bool Window::mouse_button_pressed(int glfwButton) const {
	return glfwGetMouseButton(window_, glfwButton) == GLFW_PRESS;
}