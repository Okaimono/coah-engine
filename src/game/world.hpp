#pragma once
#include "game/chunk.hpp"
#include <unordered_map>
#include <memory>

struct PerlinChunk {
    ChunkCoord coord;
    float noise[16][16];
};

extern "C" void launchNoiseMap(int x, int z, PerlinChunk* chunks);

class World {
public:
    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> worldGrid;
    std::unique_ptr<PerlinChunk[]> perlin;
    std::vector<ChunkCoord> renderedChunks;

    const int renderDist = 4;

    World() {
        generatePerlin();
        //launchNoiseMap(-5, 5, -5, 5, (float*)noise);
        for (int x = -10; x < 10; x++) {
            for (int z = -10; z < 10; z++) {
                worldGrid[{x, z}] = Chunk(x, z, (float*)perlin[(x + 10) * 20 + (z + 10)].noise);
            }
        }
    }

    void generatePerlin() {
        perlin = std::make_unique<PerlinChunk[]>(20 * 20);
        for (int x = -10; x < 10; x++) {
            for (int z = -10; z < 10; z++) {
                perlin[(x + 10) * 20 + (z + 10)].coord = {x, z};
            }
        }
        launchNoiseMap(20, 20, perlin.get());
    }

    void updateBlockAt(const glm::vec3& blockPos, int block) {
        ChunkCoord coord;
        coord.x = static_cast<int>(std::floor(blockPos.x / 16.0f));
        coord.z = static_cast<int>(std::floor(blockPos.z / 16.0f));

        auto it = worldGrid.find(coord);
        if (it == worldGrid.end()) return;

        Chunk& chunk = it->second;

        int localX = static_cast<int>(blockPos.x) - coord.x * 16;
        int localY = static_cast<int>(blockPos.y);
        int localZ = static_cast<int>(blockPos.z) - coord.z * 16;

        if (localY < 0 || localY >= chunk.height) return; 

        chunk.blocks[localX][localY][localZ] = block;
    }

    int getBlockAt(const glm::vec3& blockPos) {
        ChunkCoord coord;
        coord.x = static_cast<int>(std::floor(blockPos.x / 16.0f));
        coord.z = static_cast<int>(std::floor(blockPos.z / 16.0f));

        auto it = worldGrid.find(coord);
        if (it == worldGrid.end()) return 0;

        Chunk& chunk = it->second;

        int localX = static_cast<int>(blockPos.x) - coord.x * 16;
        int localY = static_cast<int>(blockPos.y);
        int localZ = static_cast<int>(blockPos.z) - coord.z * 16;

        if (localY < 0 || localY >= chunk.height) return 0; 

        return chunk.blocks[localX][localY][localZ];
    }

    // void getRenderChunks(glm::vec3 position) {
    //     for (int x = 0; x <= renderDist; x++) {
    //         for (int z = 0; z <= renderDist; z++) {
    //             if (x * x + z * z == renderDist * renderDist) {
    //                 ChunkCoords coords = {x, z};
    //                 auto it = worldGrid.find(coords);
    //                 if (it != worldGrid.end()) {
    //                     renderedChunks.push_back(coords);
    //                 }
    //             }
    //         }
    //     }
    // }

};

// each chunk has 16 x 16
// each block represents a chunk
// start 