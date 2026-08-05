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

    bool keyPressed(int key) const {
        return glfwGetKey(window_.get(), key) == GLFW_PRESS;
    }

    bool mouseButtonHeld(int button) const {
        return glfwGetMouseButton(window_.get(), button) == GLFW_PRESS;
    }

    bool leftClickedOnce() const   { return leftClickedOnce_; }
    bool rightClickedOnce() const  { return rightClickedOnce_; }
    bool escapePressedOnce() const { return escapePressedOnce_; }

    void update() {
        double x, y;
        glfwGetCursorPos(window_.get(), &x, &y);
        mouseDeltaX_ = x - lastX_;
        mouseDeltaY_ = y - lastY_;
        lastX_ = x;
        lastY_ = y;

        bool leftDown   = glfwGetMouseButton(window_.get(), GLFW_MOUSE_BUTTON_LEFT)  == GLFW_PRESS;
        bool rightDown  = glfwGetMouseButton(window_.get(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        bool escapeDown = glfwGetKey(window_.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS;

        leftClickedOnce_   = leftDown   && !leftWasDown_;
        rightClickedOnce_  = rightDown  && !rightWasDown_;
        escapePressedOnce_ = escapeDown && !escapeWasDown_;

        leftWasDown_  = leftDown;
        rightWasDown_ = rightDown;
        escapeWasDown_ = escapeDown;
    }

    void setCursorMode(bool captured) {
        glfwSetInputMode(window_.get(), GLFW_CURSOR,
                          captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

    double mouseX() const { return lastX_; }
    double mouseY() const { return lastY_; }

    double mouseDeltaX() const { return mouseDeltaX_; }
    double mouseDeltaY() const { return mouseDeltaY_; }

private:
    Window& window_;
    double lastX_ = 0.0, lastY_ = 0.0;
    double mouseDeltaX_ = 0.0, mouseDeltaY_ = 0.0;

    bool leftWasDown_  = false;
    bool rightWasDown_ = false;
    bool escapeWasDown_ = false;

    bool leftClickedOnce_  = false;
    bool rightClickedOnce_ = false;
    bool escapePressedOnce_ = false;
};