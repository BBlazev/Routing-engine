#ifndef WINDOW_HPP
#define WINDOW_HPP


#include <GLFW/glfw3.h>
#include <settings.hpp>

class Window{
    public:
        Window();
        ~Window();
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        
        Window(Window&& other) noexcept : window(other.window), width(other.width), height(other.height){
            other.window = nullptr;
        }

        Window& operator=(Window&& other) noexcept {
            if( this != &other){
                if(window) glfwDestroyWindow(window);
                window = other.window;
                width = other.width;
                height = other.height;
                other.window = nullptr;
            }
            return *this;
        }

        [[nodiscard]] bool should_close() const;
        [[nodiscard]] int get_width() const;
        [[nodiscard]] int get_height() const;
        [[nodiscard]] GLFWwindow* get_window() const;
    private:
        GLFWwindow* window = nullptr;
        int width = SCREEN_WIDTH;
        int height = SCREEN_HEIGHT;
};


#endif
