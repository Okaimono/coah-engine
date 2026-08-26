#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>

struct Arrow {
    glm::vec3 position;
    glm::vec3 velocity;
};

class ArrowManager {
public:
    ArrowManager(Renderer& renderer)
        : renderer_(renderer)
    {
        spawnArrow(glm::vec3(0.0f, 30.0f, 0.0f), glm::vec3(0.0f, 1.0f, -1.0f));

    }

    void spawnArrow(const glm::vec3& origin, const glm::vec3& direction) {
        Arrow arrow;
        arrow.position = origin;
        arrow.velocity = direction * ARROW_VELOCITY;
        arrows_.push_back(arrow);
    }

    void updateArrows(const float dt) {
        for (auto& arrow : arrows_) {
            arrow.velocity.y -= 9.8f * dt;

            arrow.position += arrow.velocity * dt;
        }
    }

    void renderArrows() const {
        std::vector<EntityInstance> instances;
        for (const auto& arrow : arrows_) {
            EntityInstance instance;
            instance.worldPos = arrow.position;
            instance.size = 0.2f;
            instance.rotation = orientationFromDirection(arrow.velocity);
            instances.push_back(instance);
        }

        renderer_.updateEntityInstances(instances);
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

    const float ARROW_VELOCITY = 30.0f;

    std::vector<Arrow> arrows_;
};