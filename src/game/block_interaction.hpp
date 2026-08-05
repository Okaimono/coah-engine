#pragma once
#include "game/player/player.hpp"
#include "game/world.hpp"
#include "coah_engine/input_manager.hpp"
#include "game/chunk_mesher.hpp"

class BlockInteraction {
public:
    BlockInteraction(World& world, ChunkMesher& chunkMesher, 
        InputManager& inputManager, Player& player)
        : world_(world)
        , inputManager_(inputManager)
        , chunkMesher_(chunkMesher)
        , player_(player)
    {}

    void processInput(bool menuOpen) {
        if (menuOpen) {return; }

        if (inputManager_.leftClickedOnce()) {
            breakBlock();
        }
    }

    void breakBlock() {
        glm::vec3 rayOrigin = player_.position + glm::vec3(0.0f, 1.7f, 0.0f);
        glm::vec3 rayDir    = glm::normalize(player_.orientation);

        const float step = 0.1f;   // march distance per iteration — small enough not to skip a block

        for (float t = 0.0f; t < range; t += step) {
            glm::vec3 samplePos = rayOrigin + rayDir * t;

            samplePos.x = std::floor(samplePos.x);
            samplePos.y = std::floor(samplePos.y);
            samplePos.z = std::floor(samplePos.z);

            bool hitSolidBlock = world_.getBlockAt(samplePos) != 0;
            
            if (hitSolidBlock) {
                world_.updateBlockAt(samplePos, AIR);

                ChunkCoord coord;
                coord.x = static_cast<int>(std::floor(samplePos.x / 16.0f));
                coord.z = static_cast<int>(std::floor(samplePos.z / 16.0f));

                chunkMesher_.markDirty(coord);
                break;
            }
        }
    }

private:
    World& world_;
    ChunkMesher& chunkMesher_;
    InputManager& inputManager_;
    Player& player_;

    float range = 5.0f;
};