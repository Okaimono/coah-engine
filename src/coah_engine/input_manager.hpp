#pragma once
#include "vulkan_includes.hpp"
#include "vulkan/window.hpp"

class InputManager {
public:
    InputManager(Window& window) 
        : window_(window)
    {
        glfwGetCursorPos(window.get(), &lastX_, &lastY_);
    }

    bool keyPressed(int key) {
        return glfwGetKey(window_.get(), key) == GLFW_PRESS;
    }

    void update() {
        double x, y;
        glfwGetCursorPos(window_.get(), &x, &y);
        mouseDeltaX_ = x - lastX_;
        mouseDeltaY_ = y - lastY_;
        lastX_ = x;
        lastY_ = y;
    }

    double mouseDeltaX() const { return mouseDeltaX_; }
    double mouseDeltaY() const { return mouseDeltaY_; }

private:
    Window& window_;
    double lastX_ = 0.0, lastY_ = 0.0;
    double mouseDeltaX_ = 0.0, mouseDeltaY_ = 0.0;
};