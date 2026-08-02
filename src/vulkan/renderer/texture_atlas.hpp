#pragma once
#include <unordered_map>
#include <string>
#include "core/types.hpp"  // for UVRect

class TextureAtlas {
public: 
    // Call once, after the atlas image's pixel dimensions are known
    // (e.g. right after UiPipeline loads the PNG and knows texW/texH).
    void init(uint32_t atlasWidth, uint32_t atlasHeight) {
        atlasWidth_  = (float)atlasWidth;
        atlasHeight_ = (float)atlasHeight;

        // Reserved regions — every atlas PNG must have these baked in
        // at these exact pixel coordinates by convention.
        registerRegion("white",   0, 0, 2, 2);
        registerRegion("missing", 2, 0, 8, 8);  // purple/black checker, sits next to white
    }

    void registerRegion(const std::string& name, float pixelX, float pixelY,
                         float pixelW, float pixelH) {
        UVRect uv;
        uv.u0 = pixelX / atlasWidth_;
        uv.v0 = pixelY / atlasHeight_;
        uv.u1 = (pixelX + pixelW) / atlasWidth_;
        uv.v1 = (pixelY + pixelH) / atlasHeight_;
        regions_[name] = uv;
    }

    const UVRect& get(const std::string& name) const {
        auto it = regions_.find(name);
        if (it == regions_.end()) {
            return regions_.at("missing");  // never throws — visually loud fallback instead
        }
        return it->second;
    }

    const UVRect& white()   const { return regions_.at("white"); }
    const UVRect& missing() const { return regions_.at("missing"); }

private:
    float atlasWidth_ = 1.0f, atlasHeight_ = 1.0f;
    std::unordered_map<std::string, UVRect> regions_;
};