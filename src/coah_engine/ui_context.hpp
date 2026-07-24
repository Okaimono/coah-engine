#pragma once
#include "vulkan_includes.hpp"
#include "core/config.hpp"
#include "core/types.hpp"

struct Rect {
    float x, y;
    float w, h;
};

struct QuadBatch {
    std::vector<UIVertex> vertices;

    void clear() {
        vertices.clear();
    }

    void addRect(const Rect& rect) {
        auto toNDC = [&](float x, float y) -> glm::vec2 {
            return {
                (x / WIDTH) * 2.0f - 1.0f,
                (y / HEIGHT) * 2.0f - 1.0f
            };
        };

        glm::vec2 p0 = toNDC(rect.x, rect.y);
        glm::vec2 p1 = toNDC(rect.x + rect.w, rect.y);
        glm::vec2 p2 = toNDC(rect.x + rect.w, rect.y + rect.h);
        glm::vec2 p3 = toNDC(rect.x, rect.y + rect.h);

        vertices.push_back({p0}); vertices.push_back({p1}); vertices.push_back({p2});
        vertices.push_back({p0}); vertices.push_back({p2}); vertices.push_back({p3});
    }
};

class UIContext {
public:
    void BeginFrame(float mouseX, float mouseY, bool mouseDown) {
        mouseDownLastFrame_ = mouseDown_;

        mouseX_    = mouseX;
        mouseY_    = mouseY;
        mouseDown_ = mouseDown;

        mouseCaptured_ = false;

        quadBatch_.clear();
    }

    void endFrame() {
        for (const Rect& rect : pendingRects_) {
            quadBatch_.addRect(rect);
        }
        pendingRects_.clear();
    }

    bool Button(const char* label, const Rect& bounds) {
        pendingRects_.push_back(bounds);
        return false;
    }

    const std::vector<UIVertex>& getQuadBatch() const {
        return quadBatch_.vertices;
    }

private: 
    bool mouseCaptured_ = false;
    float mouseX_, mouseY_; bool mouseDown_, mouseDownLastFrame_;

    std::vector<Rect> pendingRects_;
    QuadBatch quadBatch_;
};