#pragma once
#include "vulkan/renderer.hpp"
#include "coah_engine/input_manager.hpp"
#include "coah_engine/ui_context.hpp"
#include "game/player/player.hpp"
#include "game/player/player_inventory.hpp"

#include "game/block_interaction.hpp"
#include "game/chunk_mesher.hpp"
#include "game/game_ui.hpp"

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
        uiContext_.BeginFrame(
            static_cast<float>(inputManager_.mouseX()),
            static_cast<float>(inputManager_.mouseY()),
            inputManager_.mouseButtonHeld(GLFW_MOUSE_BUTTON_LEFT)
        );

        // PROCESS INPUTS HERE
        if (inputManager_.escapePressedOnce()) {
            menuOpen_ = !menuOpen_;
            inputManager_.setCursorMode(!menuOpen_);
        }
        player.processInput(inputManager_, dt, menuOpen_);
        blockInteraction_.processInput(menuOpen_);

        interfaceUI_.draw(uiContext_);
        hotbarUI_.draw(uiContext_, playerInventory_);
    }

    void render() {
        if (chunkMesher_.hasPendingWork()) {
            chunkMesher_.flush();
        }

        chunkMesher_.addRenderEntries();
        
        uiContext_.endFrame();
        renderer_.updateUI(uiContext_.getQuadBatch());

        glm::mat4 view = player.getViewMatrix();
        glm::mat4 proj = player.getProjectionMatrix();
        renderer_.updateUniformBuffer(view, proj);
    }
    
private:
    Renderer&     renderer_;
    InputManager& inputManager_;
    UIContext& uiContext_;

    World world;
    Player player;
    PlayerInventory playerInventory_;

    ChunkMesher chunkMesher_;
    BlockInteraction blockInteraction_;

    HotbarUI hotbarUI_;
    InterfaceUI interfaceUI_;

    bool menuOpen_ = false;

    void createChunkSlots() {
        for (auto& [key, value] : world.worldGrid) {
            value.buildMesh();
            value.slot = renderer_.reserveChunkSlot(value.faces);
        }
    }
};