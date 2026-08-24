#pragma once
#include "game/world.hpp"
#include <unordered_set>

class ChunkMesher {
public:
    ChunkMesher(Renderer& renderer, World& world) 
        : renderer_(renderer)
        , world_(world)
    {}

    void addRenderEntries() {
        for (const auto& [key, value] : world_.worldGrid) {
            ChunkRenderEntry entry;
            entry.slot = value.slot;
            entry.faceSize = static_cast<uint32_t>(value.faces.size() * 6);
            entry.chunkModel = glm::translate(glm::mat4(1.0f), glm::vec3((float)key.x * 16.0f, 0.0f, (float)key.z * 16.0f));

            renderer_.addRenderEntry(entry);
        }
    }

    bool hasPendingWork() const {
        return !dirtyChunks_.empty();
    }

    void flush() {
        for (const auto& coord : dirtyChunks_) {
            auto it = world_.worldGrid.find(coord);
            if (it == world_.worldGrid.end()) continue;
            Chunk& chunk = it->second;
            chunk.buildMesh();

            renderer_.updateChunkSlot(chunk.slot, chunk.faces);
        }
        dirtyChunks_.clear();
    }

    void markDirty(ChunkCoord coord) {
        dirtyChunks_.insert(coord);
    }

private:
    Renderer& renderer_;
    World& world_;

    std::unordered_set<ChunkCoord, ChunkCoordHash> dirtyChunks_;
};