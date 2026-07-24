#pragma once
#include "vulkan/renderer.hpp"
#include "coah_engine/input_manager.hpp"
#include "coah_engine/ui_context.hpp"
#include "game/player/player.hpp"
#include "game/block_interaction.hpp"
#include "game/chunk_mesher.hpp"

class CallOfAHero {
public:
    CallOfAHero(Renderer& renderer, InputManager& inputManager, UIContext& uiContext) 
        : renderer_(renderer)
        , inputManager_(inputManager)
        , uiContext_(uiContext)
        , chunkMesher_(renderer_, world)
        , blockInteraction_(world, chunkMesher_, inputManager_, player)
    {
        createChunkSlots();
    }

    void update(float dt) {
        player.processInput(inputManager_, dt);
        blockInteraction_.update();

        Rect rect;
        rect.x = 700.0f;
        rect.y = 0.0f;
        rect.w = 300.0f;
        rect.h = 600.0f;

        uiContext_.Button("test", rect);
    }

    void render() {
        if (chunkMesher_.hasPendingWork()) {
            chunkMesher_.flush();
        }

        uiContext_.endFrame();

        glm::mat4 view = player.getViewMatrix();
        glm::mat4 proj = player.getProjectionMatrix();
        renderer_.updateUniformBuffer(view, proj);
        renderer_.drawFrame(world);
    }
    
private:
    Renderer&     renderer_;
    InputManager& inputManager_;
    UIContext& uiContext_;

    World world;
    Player player;
    ChunkMesher chunkMesher_;
    BlockInteraction blockInteraction_;

    void createChunkSlots() {
        for (auto& [key, value] : world.worldGrid) {
            value.buildMesh();
            value.slot = renderer_.reserveChunkSlot(value.faces);
        }
    }
};