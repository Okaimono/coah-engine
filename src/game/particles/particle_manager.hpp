#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include "vulkan/renderer.hpp"
#include <vector>

#include <random>

std::mt19937 rng(std::random_device{}());

float randomFloat(float min, float max) {
    return std::uniform_real_distribution<float>(min, max)(rng);
}

// Create a effect with typename Derivedd

struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    float lifetime = 3.0f;
};

class ParticleManager {
public:
    ParticleManager(Renderer& renderer)
        : renderer_(renderer)
    {}

    void spawnEffect(const glm::vec3& origin, const glm::vec3& direction) {
        glm::vec3 dir = glm::normalize(direction);
        glm::vec3 perp = findPerpendicular(dir);

        const int count = 1;

        for (int i = 0; i < count; i++) {
            Particle particle;

            float angle = glm::radians(360.0f / count * i);

            glm::vec3 spokeDir = glm::rotate(perp, angle, dir); 
            
            float radius = 0.1f;
            spokeDir *= radius;

            particle.position = origin + spokeDir;
            particle.velocity = spokeDir * 10.0f;

            particles.push_back(particle);
        }
    }

    void addParticle(const glm::vec3& origin, const glm::vec3& direction) {
        glm::vec3 dir = glm::normalize(direction);
        glm::vec3 perp = findPerpendicular(dir);

        float rand = randomFloat(0.0f, 1.0f);
        float angle = glm::radians(360.0f * rand);

        glm::vec3 spokeDir = glm::rotate(perp, angle, dir);

        Particle particle;
        particle.position = origin + spokeDir;
        particle.velocity = spokeDir * 10.0f;
        particles.push_back(particle);
        
    }

    glm::vec3 findPerpendicular(glm::vec3 v) {
        v = glm::normalize(v);
        // pick an arbitrary vector not parallel to v
        glm::vec3 arbitrary = (glm::abs(v.x) < 0.99f) ? glm::vec3(1,0,0) : glm::vec3(0,1,0);
        return glm::normalize(glm::cross(v, arbitrary));
    }

    void updateParticles(const float dt) {
        std::vector<Particle> aliveParticles;
        for (auto& particle : particles) {
            particle.position += particle.velocity * dt;
            particle.lifetime -= dt;

            if (particle.lifetime > 0) {
                aliveParticles.push_back(particle);
            }
        }

        particles = aliveParticles;
    }

    void renderParticles() const {
        std::vector<ParticleInstance> particleInstances;

        for (const auto& particle : particles) {
            ParticleInstance instance;
            instance.worldPos = particle.position;
            instance.size = .4f;
            instance.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
            instance.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

            particleInstances.push_back(instance);
        }
        renderer_.updateParticleInstances(particleInstances);   
    }

private:
    Renderer& renderer_;

    std::vector<Particle> particles;
};