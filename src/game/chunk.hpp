#pragma once
#include "vulkan_includes.hpp"
#include "vulkan/pool_allocator.hpp"
#include <cmath>

enum Block { AIR, COBBLESTONE, DIRT, GRASS };

struct ChunkCoord {
    int x, z;
    bool operator==(const ChunkCoord& other) const {
        return x == other.x && z == other.z;
    }
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& v) const noexcept {
        size_t h = 17;
        h = h * 31 + std::hash<int>{}(v.x);
        h = h * 31 + std::hash<int>{}(v.z);
        return h;
    }
};

const glm::ivec3 adjacentSide[6] = {
    glm::ivec3(0,  1,  0),
    glm::ivec3(0, -1,  0),
    glm::ivec3(0,  0, -1),
    glm::ivec3(0,  0,  1),
    glm::ivec3(1,  0,  0),
    glm::ivec3(-1, 0,  0),
};

class Chunk {
public:
    int width = 16, height = 32, depth = 16;
    int blocks[16][32][16] {};
    std::vector<uint32_t> faces;
    Slot slot;

    Chunk() {}

    Chunk(int chunkX, int chunkZ, float* noise) {
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                float n = noise[x * 16 + z]; // -1 to 1
                int surfaceY = (int)((n + 1.0f) * 0.5f * 24) + 4; // 4..28
                surfaceY = glm::clamp(surfaceY, 1, 31);

                for (int y = 0; y < height; y++) {
                    if      (y < surfaceY - 3) blocks[x][y][z] = COBBLESTONE;
                    else if (y < surfaceY)     blocks[x][y][z] = DIRT;
                    else if (y == surfaceY)    blocks[x][y][z] = GRASS;
                    else                       blocks[x][y][z] = AIR;
                }
            }
        }
        buildMesh();
    }
    
    void buildMesh() {
        faces.clear();
        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 32; y++) {
                for (int z = 0; z < 16; z++) {
                    if (blocks[x][y][z] == 0) continue;
                    for (int i = 0; i < 6; i++) {
                        if (!isSolid(i, glm::ivec3(x, y, z))) {
                            uint32_t data = 0;
                            data |= (x & 0xF)                  << 0;
                            data |= (y & 0x1F)                 << 4;
                            data |= (z & 0xF)                  << 9;
                            data |= (i & 0x7)                  << 13;
                            data |= (blocks[x][y][z] & 0xF)    << 16;
                            faces.push_back(data);
                        }
                    }
                }
            }
        }
        faces.resize(50000, 0);
    }

private:
    bool isSolid(int face, const glm::ivec3& position) {
        glm::ivec3 n = position + adjacentSide[face];
        if (n.x < 0 || n.x >= 16 ||
            n.y < 0 || n.y >= 32 ||
            n.z < 0 || n.z >= 16)
            return false;
        return blocks[n.x][n.y][n.z] != 0;
    }
};

// #pragma once
// #include "vulkan_includes.hpp"
// #include "vulkan/pool_allocator.hpp"

// enum Block {
//     AIR,
//     COBBLESTONE,
//     DIRT,
//     GRASS
// };

// struct ChunkCoord {
//     int x;
//     int z;
//     bool operator==(const ChunkCoord& other) const {
//         return x == other.x && z == other.z;
//     }
// };

// struct ChunkCoordHash {
//     size_t operator()(const ChunkCoord& v) const noexcept {
//         return std::hash<int>{}(v.x) ^ 
//                 (std::hash<int>{}(v.z) << 1);
//     }
// };

// const glm::ivec3 adjacentSide[6] = {
//     glm::ivec3(0,  1,  0),  // 0 ABOVE
//     glm::ivec3(0, -1,  0),  // 1 BELOW
//     glm::ivec3(0,  0, -1),  // 2 NORTH (-Z, matches shader face 2)
//     glm::ivec3(0,  0,  1),  // 3 SOUTH (+Z, matches shader face 3)
//     glm::ivec3(1,  0,  0),  // 4 EAST
//     glm::ivec3(-1, 0,  0),  // 5 WEST
// };

// class Chunk {
// public:
//     int width = 16, height = 32, depth = 16;
//     int blocks[16][32][16] {};
//     std::vector<uint32_t> faces;

//     Slot slot;

//     Chunk() {}

//     Chunk(int xCoord, int zCoord) {
//         for (int x = 0; x < width; x++) {
//             for (int y = 0; y < height; y++) {
//                 for (int z = 0; z < depth; z++) {
//                     if (xCoord == 2) {
//                         blocks[x][y][z] = GRASS;
//                     }
//                 }
//             }
//         }

//         for (int x = 0; x < 16; x++) {
//             for (int y = 0; y < 32; y++) {
//                 for (int z = 0; z < 16; z++) {
//                     for (int i = 0; i < 6; i++) {
//                         if (blocks[x][y][z] == 0) continue;
//                         glm::ivec3 position = glm::ivec3(x, y, z);

//                         if (!isSolid(i, position)) {
//                             uint32_t data = 0;
//                             data |= (x & 0xF)  << 0;
//                             data |= (y & 0x1F) << 4;
//                             data |= (z & 0xF)  << 9;
//                             data |= (i & 0x7)  << 13;
//                             data |= (blocks[x][y][z] & 0xF) << 16;
//                             faces.push_back(data);
//                         }
//                     }
//                 }
//             }
//         }
//     }

// private:
//     bool isSolid(int face, const glm::ivec3& position) {
//         glm::ivec3 n = position + adjacentSide[face];

//         if (n.x < 0 || n.x >= 16 ||
//             n.y < 0 || n.y >= 32 ||
//             n.z < 0 || n.z >= 16)
//             return false;

//         return blocks[n.x][n.y][n.z] != 0;
//     }
// };