#pragma once
#include "vulkan/renderer.hpp"
#include "vulkan_includes.hpp"
#include "game/entities/entity_id_generator.hpp"

#include <iostream>
#include <vector>

class Basilisk {
public:
    struct BasiliskSegment {
        glm::vec3 worldPos;
        float size = 5.0f;
        glm::vec3 orientation;
        uint32_t entityId;
    };

    std::vector<BasiliskSegment> segments;
    const int segmentCount = 10;
    float HP = 10000.0f;

    const float maxVelocity = 100.0f;
    const float steerGain = 2.0f;
    glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 target = glm::vec3(100.0f, 0.0f, 100.0f);

    Basilisk(glm::vec3 spawnPos, glm::vec3 orientation, EntityIdGenerator& idGen) {
        for (int i = 0; i < segmentCount; i++) {
            BasiliskSegment segment;
            segment.entityId = idGen.next();
            segment.worldPos = spawnPos + glm::vec3(0.0f, 0.0f, i * (-segment.size - 0.2f));
            segment.orientation = orientation;
            segments.push_back(segment);
        }
    }

    void hitDetected() {
        HP -= 100.0f;
    }

    void update(const float dt, const glm::vec3& playerPos) {
        target = playerPos;
        updateVelocity(dt);
        updateSegments(dt);
    }

private:
    void updateVelocity(const float dt) {
        BasiliskSegment& head = segments[0];

        glm::vec3 toTarget = target - head.worldPos;
        float dist = glm::length(toTarget);

        if (dist > 0.0001f) {
            glm::vec3 desiredVelocity = (toTarget / dist) * maxVelocity;  // normalize, scale to max speed
            glm::vec3 steer = (desiredVelocity - velocity) * glm::min(steerGain * dt, 1.0f);
            velocity += steer;

            // clamp so velocity never exceeds maxVelocity, regardless of how strong the steer was
            float speed = glm::length(velocity);
            if (speed > maxVelocity) {
                velocity = (velocity / speed) * maxVelocity;
            }
        }

        if (glm::length(velocity) > 0.0001f) {
            head.orientation = glm::normalize(velocity);
        }
    }

    void updateSegments(const float dt) {
        BasiliskSegment& head = segments[0];
        head.worldPos += velocity * dt;

        for (int i = 1; i < segmentCount; i++) {
            glm::vec3 dist = segments[i - 1].worldPos - segments[i].worldPos;
            glm::vec3 move = glm::normalize(dist) * segments[i].size;
            segments[i].worldPos = segments[i - 1].worldPos - move;
            segments[i].orientation = glm::normalize(dist);
        }
    }
};

class BasiliskManager {
public:
    BasiliskManager(EntityIdGenerator& idGen) 
        : idGen_(idGen)
    {
        basilisks_.emplace_back(glm::vec3(0.0f, 50.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), idGen_);
    }

    void updateBasilisk(const float dt, const glm::vec3& playerPos) {
        for (auto& basilisk : basilisks_) {
            if (basilisk.HP <= 0) {
                basilisks_.pop_back();
            }
            basilisk.update(dt, playerPos);
        }
    }

    void getEntityInstances(std::vector<EntityInstance>& instances) {
        for (const auto& basilisk: basilisks_) {
            for (const auto& segment : basilisk.segments) {
                EntityInstance instance;
                instance.worldPos = segment.worldPos;
                instance.size = segment.size;
                instance.rotation = rotateSegment(segment.orientation);
                instances.push_back(instance);
            }
        }
    }

    glm::vec4 rotateSegment(const glm::vec3& direction) const {
        glm::vec3 forward = glm::normalize(direction);
        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

        if (glm::abs(glm::dot(forward, worldUp)) > 0.999f) {
            worldUp = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        glm::quat q = glm::quatLookAtRH(forward, worldUp);
        return glm::vec4(q.x, q.y, q.z, q.w);
    }

    std::vector<Basilisk>& getBasilisks() { return basilisks_; }

private:
    EntityIdGenerator& idGen_;

    std::vector<Basilisk> basilisks_;
};