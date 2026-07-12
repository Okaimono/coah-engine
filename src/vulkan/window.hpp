#pragma once
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cassert>

class Window {
public:
    Window(int width, int height, const char* title) {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        handle_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!handle_) { glfwTerminate(); throw std::runtime_error("window creation failed"); }
        glfwSetInputMode(handle_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    ~Window() { 
        glfwDestroyWindow(handle_);
        glfwTerminate();
    }
    
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    GLFWwindow* get() const { 
        assert(handle_ && "Window variant violated");
        return handle_; 
    }
private:
    GLFWwindow* handle_ = nullptr;
};