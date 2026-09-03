#pragma once
#include "coah_engine/input_manager.hpp"
#include "game/player/player.hpp"
#include "game/player/player_inventory.hpp"
#include "game/entities/arrow_manager.hpp"

class PlayerInteraction {
public:
    PlayerInteraction(InputManager& inputManager, Player& player, PlayerInventory& playerInventory, ArrowManager& arrowManager) 
        : inputManager_(inputManager)
        , player_(player)
        , playerInventory_(playerInventory)
        , arrowManager_(arrowManager)
    {}
 
    void processInput(const float dt) {
        if (inputManager_.rightClickedOnce()) {
            arrowManager_.spawnArrow(player_.getEyePosition(), player_.orientation);
        }
    }

private:
    InputManager& inputManager_;
    Player& player_;
    PlayerInventory& playerInventory_;
    ArrowManager& arrowManager_;
};