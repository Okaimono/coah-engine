#pragma once
#include "vulkan/renderer.hpp"
#include "game/particles/particle_manager.hpp"
#include "game/entities/arrow_manager.hpp"
#include "game/entities/basilisk_manager.hpp"
#include "game/entities/entity_id_generator.hpp"
#include "game/player/player.hpp"


struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

inline AABB makeAABB(const glm::vec3& position, float size) {
    glm::vec3 half(size * 0.5f);
    return { position - half, position + half };
}

inline bool intersects(const AABB& a, const AABB& b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

class EntityManager {
public:
    EntityManager(Renderer& renderer, ParticleManager& particleManager, Player& player)
        : renderer_(renderer)
        , particleManager_(particleManager)
        , basiliskManager_(idGen_)
        , arrowManager_(particleManager, idGen_)
        , player_(player)
    {}

    void update(float dt) {
        arrowManager_.updateArrows(dt);
        basiliskManager_.updateBasilisk(dt, player_.position);
        collisions();
    }

    void collisions() {
        auto& liveArrows = arrowManager_.getArrows();
        auto& basilisks = basiliskManager_.getBasilisks();

        for (size_t a = 0; a < liveArrows.size(); ++a) {
            auto& arrow = liveArrows[a];
            AABB arrowBox = makeAABB(arrow.position, ArrowManager::ARROW_SIZE);

            for (size_t b = 0; b < basilisks.size(); ++b) {
                auto& basilisk = basilisks[b];
                auto& segments = basilisk.segments;

                for (size_t s = 0; s < segments.size(); ++s) {
                    auto& segment = segments[s];
                    AABB segBox = makeAABB(segment.worldPos, segment.size);

                    if (intersects(arrowBox, segBox)) {
                        bool alreadyHit = std::find(arrow.hitSegmentIds.begin(), arrow.hitSegmentIds.end(),
                            segment.entityId) != arrow.hitSegmentIds.end();
                        if (!alreadyHit) {
                            arrow.addHitSegmentId(segment.entityId);
                            basilisk.hitDetected();
                        }
                    }
                }
            }
        }
    }

    void render() {
        std::vector<EntityInstance> entityInstances;
        arrowManager_.getEntityInstances(entityInstances);
        basiliskManager_.getEntityInstances(entityInstances);
        renderer_.updateEntityInstances(entityInstances);
    }

    ArrowManager& getArrowManager() {return arrowManager_; }
private:
    Renderer& renderer_;
    ParticleManager& particleManager_;

    EntityIdGenerator idGen_;
    ArrowManager arrowManager_;
    BasiliskManager basiliskManager_;

    Player& player_;
    
};