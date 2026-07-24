#pragma once
#include "game/world.hpp"
#include <unordered_set>

class ChunkMesher {
public:
    ChunkMesher(Renderer& renderer, World& world) 
        : renderer_(renderer)
        , world_(world)
    {}

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