#pragma once
#include "vulkan/renderer.hpp"
#include "coah_engine/input_manager.hpp"
#include "coah_engine/ui_context.hpp"
#include "game/player/player.hpp"
#include "game/player/player_inventory.hpp"
#include "game/player/player_interaction.hpp"
#include "game/entities/arrow_manager.hpp"

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
        , particleManager_(renderer)
        , arrowManager_(renderer, particleManager_)
        , playerInteraction_(inputManager, player, playerInventory_, arrowManager_)
    {
        createChunkSlots();
    }

    void update(float dt) {
        // make UIManager here
        uiContext_.BeginFrame(
            static_cast<float>(inputManager_.mouseX()),
            static_cast<float>(inputManager_.mouseY()),
            inputManager_.mouseButtonHeld(GLFW_MOUSE_BUTTON_LEFT)
        );

        // Process inputs (make PlayerInteraction head here)
        if (inputManager_.escapePressedOnce()) {
            menuOpen_ = !menuOpen_;
            inputManager_.setCursorMode(!menuOpen_);}
        player.processInput(inputManager_, dt, menuOpen_);
        blockInteraction_.processInput(menuOpen_);
        playerInteraction_.processInput(dt);

        // Update entities (make EntityManager)
        arrowManager_.updateArrows(dt);
        particleManager_.updateParticles(dt);
        
        // Update UI (make UIManager here)
        interfaceUI_.draw(uiContext_);
        hotbarUI_.draw(uiContext_, playerInventory_);
    }

    void render() {
        // update Chunks (make ChunkManager)
        if (chunkMesher_.hasPendingWork()) {
            chunkMesher_.flush();
        }
        chunkMesher_.addRenderEntries();
        
        // update UI stuff
        uiContext_.endFrame();
        renderer_.updateUI(uiContext_.getQuadBatch());

        // render entities
        arrowManager_.renderArrows();

        // render particles
        particleManager_.renderParticles();

        // update matricies
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
    PlayerInteraction playerInteraction_;

    ParticleManager particleManager_;
    ArrowManager arrowManager_;

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