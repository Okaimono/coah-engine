#pragma once
#include <cstdint>

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

struct UIVertex {
    glm::vec2 pos;
};

struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
};

const std::vector<Vertex> vertices = {
    // front
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}},
    // back
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}},
};

const std::vector<uint32_t> indices = {
    0, 1, 2,  2, 3, 0,   // front
    5, 4, 7,  7, 6, 5,   // back
    4, 0, 3,  3, 7, 4,   // left
    1, 5, 6,  6, 2, 1,   // right
    3, 2, 6,  6, 7, 3,   // top
    4, 5, 1,  1, 0, 4,   // bottom
};