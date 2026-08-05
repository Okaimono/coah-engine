#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <chrono>

#include "vulkan/window.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/swapchain.hpp"
#include "vulkan/renderer.hpp"
#include "core/config.hpp"
#include "game/coah.hpp"
#include "coah_engine/input_manager.hpp"
#include "coah_engine/ui_context.hpp"

class CoahEngine {
public:
    CoahEngine() 
        : window_(Config::SCREEN_WIDTH, Config::SCREEN_HEIGHT, "coah")
        , ctx_(window_)
        , swapchain_(ctx_)
        , renderer_(ctx_, swapchain_)
        , inputManager_(window_)
        , coah_(renderer_, inputManager_, uiContext_)
    {}

    void run() {
        float lastTime = static_cast<float>(glfwGetTime());
        while (!glfwWindowShouldClose(window_.get())) {
            float currentTime = static_cast<float>(glfwGetTime());
            float dt = currentTime - lastTime;
            lastTime = currentTime;

            glfwPollEvents();
            inputManager_.update();
            coah_.update(dt);

            // Update all renderer data all at once rather than updateUI()
            coah_.render();
        }
        vkDeviceWaitIdle(ctx_.device);
    }

private:
    Window         window_;
    VulkanContext  ctx_;
    Swapchain      swapchain_;
    Renderer       renderer_;
    InputManager   inputManager_;
    UIContext      uiContext_;
    CallOfAHero    coah_;
};