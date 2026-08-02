#pragma once
#include "vulkan_includes.hpp"
#include "core/config.hpp"
#include "core/types.hpp"

struct QuadBatch {
    std::vector<UIVertex> vertices;

    void clear() { vertices.clear(); }

    void addRect(const Rect& rect, const glm::vec4& color, const UVRect& uv) {
        auto toNDC = [&](float x, float y) -> glm::vec2 {
            return { (x / WIDTH) * 2.0f - 1.0f, (y / HEIGHT) * 2.0f - 1.0f };
        };

        glm::vec2 p0 = toNDC(rect.x, rect.y);
        glm::vec2 p1 = toNDC(rect.x + rect.w, rect.y);
        glm::vec2 p2 = toNDC(rect.x + rect.w, rect.y + rect.h);
        glm::vec2 p3 = toNDC(rect.x, rect.y + rect.h);

        glm::vec2 uv0{uv.u0, uv.v0}, uv1{uv.u1, uv.v0}, uv2{uv.u1, uv.v1}, uv3{uv.u0, uv.v1};

        vertices.push_back({p0, uv0, color});
        vertices.push_back({p1, uv1, color});
        vertices.push_back({p2, uv2, color});
        vertices.push_back({p0, uv0, color});
        vertices.push_back({p2, uv2, color});
        vertices.push_back({p3, uv3, color});
    }
};

struct Entry {
    Rect bounds;
    glm::vec4 color;
    UVRect uv;
};

struct ClickResult {
    bool clicked = false;
    float localX = 0.0f;
    float localY = 0.0f;
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
        for (const Entry& entry : pendingButtons_) {
            quadBatch_.addRect(entry.bounds, entry.color, entry.uv);
        }
        pendingButtons_.clear();
    }

    // Solid-color button — uv defaults to whole-texture, irrelevant against the 1x1 white default
    bool Button(const char* label, const Rect& bounds,
                const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f}) {
        pendingButtons_.push_back({bounds, color, UVRect{}});
        bool hovered = hitTest(bounds);
        return hovered && justClicked();
    }

    bool ImageButton(const char* label, const Rect& bounds, const UVRect& uv,
               const glm::vec4& tint = {1.0f, 1.0f, 1.0f, 1.0f}) {
        pendingButtons_.push_back({bounds, tint, uv});
        bool hovered = hitTest(bounds);
        return hovered && justClicked();
    }

    ClickResult ImageArea(const char* label, const Rect& bounds, const UVRect& uv,
                          const glm::vec4& tint = {1.0f, 1.0f, 1.0f, 1.0f}) {
        pendingButtons_.push_back({bounds, tint, uv});
        bool hovered = hitTest(bounds);

        ClickResult result;
        if (hovered && justClicked()) {
            result.clicked = true;
            result.localX = mouseX_ - bounds.x;
            result.localY = mouseY_ - bounds.y;
        }
        return result;
    }

    bool hitTest(const Rect& bounds) {
        bool hovered = !mouseCaptured_ &&
            mouseX_ >= bounds.x && mouseX_ <= bounds.x + bounds.w &&
            mouseY_ >= bounds.y && mouseY_ <= bounds.y + bounds.h;

        if (hovered) {
            mouseCaptured_ = true;
        }
        return hovered;
    }

    bool justClicked() const {
        return mouseDown_ && !mouseDownLastFrame_;
    }

    const std::vector<UIVertex>& getQuadBatch() const { return quadBatch_.vertices; }

private:
    bool mouseCaptured_ = false;
    float mouseX_, mouseY_;
    bool mouseDown_, mouseDownLastFrame_;

    std::vector<Entry> pendingButtons_;
    QuadBatch quadBatch_;
};