#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include "game/particles/particle_manager.hpp"


// Next, create an EntityManager, which will manage all entities,
// grab all the data needed,
// then push it into renderer

struct Arrow {
    glm::vec3 position;
    glm::vec3 velocity;
    float lifetime = 3.0f;
};

// Make some universal particle effect system

class ArrowManager {
public:
    ArrowManager(Renderer& renderer, ParticleManager& particleManager)
        : renderer_(renderer)
        , particleManager_(particleManager)
    {}

    void spawnArrow(const glm::vec3& origin, const glm::vec3& direction) {
        Arrow arrow;
        arrow.position = origin;
        arrow.velocity = direction * ARROW_VELOCITY;
        arrows_.push_back(arrow);
    }   

    void updateArrows(const float dt) {
        std::vector<Arrow> aliveArrows;
        for (auto& arrow : arrows_) {
            arrow.velocity.y -= 9.8f * dt;
            arrow.position += arrow.velocity * dt;
            arrow.lifetime -= dt;
            if (arrow.lifetime > 0) {
                aliveArrows.push_back(arrow);
            }
            particleManager_.addParticle(arrow.position, arrow.velocity);
        }
        arrows_ = aliveArrows;

    }

    void renderArrows() const {
        std::vector<EntityInstance> entityInstances;

        for (const auto& arrow : arrows_) {
            EntityInstance instance;
            instance.worldPos = arrow.position;
            instance.size = 1.5f;
            instance.rotation = orientationFromDirection(arrow.velocity);
            entityInstances.push_back(instance);
        }

        renderer_.updateEntityInstances(entityInstances);
        //renderer_.updateParticleInstances(instances);
    }

    glm::vec4 orientationFromDirection(const glm::vec3& direction) const {
        glm::vec3 forward = glm::normalize(direction);
        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

        if (glm::abs(glm::dot(forward, worldUp)) > 0.999f) {
            worldUp = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        glm::quat q = glm::quatLookAtRH(forward, worldUp);
        glm::vec4 result = glm::vec4(q.x, q.y, q.z, q.w);
        
        return result;
    }

private:
    Renderer& renderer_;
    ParticleManager& particleManager_;

    const float ARROW_VELOCITY = 100.0f;

    std::vector<Arrow> arrows_;
};