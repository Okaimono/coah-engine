#pragma once
#include "vulkan/renderer.hpp"
#include "vulkan_includes.hpp"

#include <iostream>
#include <vector>


class Basilisk {
public:
    struct BasiliskSegment {
        glm::vec3 worldPos;
        float size = 1.0f;
        glm::vec3 orientation;
    };

    std::vector<BasiliskSegment> segments;
    float HP = 100.0f;

    Basilisk(glm::vec3 spawnPos, glm::vec3 orientation) {
        for (int i = 0; i < 10; i++) {
            BasiliskSegment segment;
            segment.worldPos = spawnPos + glm::vec3(0.0f, 0.0f, i * -1.2f);
            segment.orientation = orientation;
            segments.push_back(segment);
        }
    }
};

class BasiliskManager {
public:
    BasiliskManager() 
    {
        basilisks_.emplace_back(glm::vec3(0.0f, 50.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    }

    void updateBasilisk(float dt) {}

    void getEntityInstances(std::vector<EntityInstance>& instances) {
        for (const auto& segment : basilisks_[0].segments) {
            EntityInstance instance;
            instance.worldPos = segment.worldPos;
            instance.size = segment.size;
            instance.rotation = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
            instances.push_back(instance);
        }
    }
    std::vector<Basilisk>& getBasilisks() { return basilisks_; }


    void hitDetected(size_t basiliskIndex, size_t segmentIndex) {
        // your actual response logic goes here — e.g.:
        std::cout<<"Hit detected" <<"\n";
    }
private:
    std::vector<Basilisk> basilisks_;
};