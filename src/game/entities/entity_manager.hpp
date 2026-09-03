#pragma once
#include "vulkan/renderer.hpp"
#include "game/particles/particle_manager.hpp"
#include "game/entities/arrow_manager.hpp"
#include "game/entities/basilisk_manager.hpp"

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
    EntityManager(Renderer& renderer, ParticleManager& particleManager)
        : renderer_(renderer)
        , particleManager_(particleManager)
        , arrowManager_(particleManager)
    {}

    void update(float dt) {
        arrowManager_.updateArrows(dt);
        basiliskManager_.updateBasilisk(dt);
        collisions();
    }

    void collisions() {
        const auto& liveArrows     = arrowManager_.getArrows();
        auto&       basilisks  = basiliskManager_.getBasilisks();

        for (size_t a = 0; a < liveArrows.size(); ++a) {
            AABB arrowBox = makeAABB(liveArrows[a].position, ArrowManager::ARROW_SIZE);

            for (size_t b = 0; b < basilisks.size(); ++b) {
                auto& segments = basilisks[b].segments;

                for (size_t s = 0; s < segments.size(); ++s) {
                    AABB segBox = makeAABB(segments[s].worldPos, segments[s].size);

                    if (intersects(arrowBox, segBox)) {
                        basiliskManager_.hitDetected(b, s);
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

    ArrowManager arrowManager_;
    BasiliskManager basiliskManager_;
    
};