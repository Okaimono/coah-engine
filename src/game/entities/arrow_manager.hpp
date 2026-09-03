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
    static constexpr float ARROW_VELOCITY = 100.0f;
    static constexpr float ARROW_SIZE     = 1.5f;

    ArrowManager(ParticleManager& particleManager)
        : particleManager_(particleManager)
    {}

    void spawnArrow(const glm::vec3& origin, const glm::vec3& direction) {
        Arrow arrow;
        arrow.position = origin;
        arrow.velocity = direction * ARROW_VELOCITY;
        arrows_.push_back(arrow);
    }

    void updateArrows(const float dt) {
        for (auto& arrow : arrows_) {
            arrow.velocity.y -= 9.8f * dt;
            arrow.position   += arrow.velocity * dt;
            arrow.lifetime   -= dt;
            particleManager_.addParticle(arrow.position, arrow.velocity);
        }
        arrows_.erase(
            std::remove_if(arrows_.begin(), arrows_.end(),
                [](const Arrow& a) { return a.lifetime <= 0.0f; }),
            arrows_.end()
        );
    }

    void getEntityInstances(std::vector<EntityInstance>& entityInstances) const {
        for (const auto& arrow : arrows_) {
            EntityInstance instance;
            instance.worldPos = arrow.position;
            instance.size     = ARROW_SIZE;
            instance.rotation = orientationFromDirection(arrow.velocity);
            entityInstances.push_back(instance);
        }
    }

    glm::vec4 orientationFromDirection(const glm::vec3& direction) const {
        glm::vec3 forward = glm::normalize(direction);
        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

        if (glm::abs(glm::dot(forward, worldUp)) > 0.999f) {
            worldUp = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        glm::quat q = glm::quatLookAtRH(forward, worldUp);
        return glm::vec4(q.x, q.y, q.z, q.w);
    }

    const std::vector<Arrow>& getArrows() const { return arrows_; }

private:
    ParticleManager& particleManager_;
    std::vector<Arrow> arrows_;
};