#pragma once
#include "vulkan_includes.hpp"
#include "coah_engine/input_manager.hpp"
#include <glm/gtc/matrix_transform.hpp>

class Player {
public:
    glm::vec3 position    = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 orientation = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up          = glm::vec3(0.0f, 1.0f, 0.0f);
    
    float speed       = 20.0f;
    float sensitivity = 0.1f;
    float yaw         = -135.0f;
    float pitch       = -20.0f;
    float fov         = 60.0f;
    float lastX       = WIDTH / 2.0f;
    float lastY       = HEIGHT / 2.0f;
    bool firstMouse   = true;

    void processInput(InputManager& input, float dt) {
        onMouseMove(input.mouseDeltaX(), input.mouseDeltaY());
        if (input.keyPressed(GLFW_KEY_W))          position += speed * dt * orientation;
        if (input.keyPressed(GLFW_KEY_S))          position -= speed * dt * orientation;
        if (input.keyPressed(GLFW_KEY_A))          position -= glm::normalize(glm::cross(orientation, up)) * speed * dt;
        if (input.keyPressed(GLFW_KEY_D))          position += glm::normalize(glm::cross(orientation, up)) * speed * dt;
        if (input.keyPressed(GLFW_KEY_SPACE))      position += up * speed * dt;
        if (input.keyPressed(GLFW_KEY_LEFT_SHIFT)) position -= up * speed * dt;
    }

    void onMouseMove(double dx, double dy) {
        if (dx == 0.0 && dy == 0.0) return;   // mouse didn't move — skip everything below

        yaw   += static_cast<float>(dx) * sensitivity;
        pitch  = glm::clamp(pitch + static_cast<float>(-dy) * sensitivity, -89.0f, 89.0f);

        orientation = glm::normalize(glm::vec3(
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            sin(glm::radians(pitch)),
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))
        ));
    }

    glm::mat4 getViewMatrix() {
        glm::vec3 eyePos = position + glm::vec3(0.0f, 1.7f, 0.0f);
        return glm::lookAt(eyePos, eyePos + orientation, up);
    }

    glm::mat4 getProjectionMatrix() {
        glm::mat4 proj = glm::perspective(glm::radians(fov), 1600.0f / 900.0f, 0.1f, 1000.0f);
        proj[1][1] *= -1;
        return proj;
    }
};