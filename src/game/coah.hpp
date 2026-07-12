#pragma once
#include "vulkan/renderer.hpp"
#include "game/player/player.hpp"

class CallOfAHero {
public:
    World world;
    Player player;

    CallOfAHero(Renderer& renderer, InputManager& inputManager) 
        : renderer_(renderer)
        , inputManager_(inputManager)
    {
        createChunkSlots();
    }

    void update(float dt) {
        player.processInput(inputManager_, dt);
    }

    void render() {
        glm::mat4 view = player.getViewMatrix();
        glm::mat4 proj = player.getProjectionMatrix();
        renderer_.updateUniformBuffer(view, proj);

        renderer_.drawFrame(world);
    }

private:
    Renderer&     renderer_;
    InputManager& inputManager_;

    void createChunkSlots() {
        for (auto& [key, value] : world.worldGrid) {
            value.slot = renderer_.reserveChunkSlot(value.faces);
        }
    }
};