#include <vulkan/window.hpp>
#include <iostream>

Window::Window(){
    if(!glfwInit()){
        throw std::runtime_error("Failed to init GLFW\n");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width, height, "Vulkan Engine", nullptr, nullptr);
    if(!window){
        glfwTerminate();
        throw std::runtime_error("Failed to create window\n");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, VulkanContext::framebuffer_resize_callback);
}

Window::~Window(){
    if(window) glfwDestroyWindow(window);
    glfwTerminate();
}

bool Window::should_close() const {
    return glfwWindowShouldClose(window);
}

int Window::get_width() const {
    return width;
}

int Window::get_height() const {
    return height;
}

GLFWwindow* Window::get_window() const {
    return window;
}