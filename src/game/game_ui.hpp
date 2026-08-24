#pragma once
#include "coah_engine/ui_context.hpp"
#include "core/config.hpp"
#include "game/player/player_inventory.hpp"
#include <string>

class InterfaceUI {
public:
    void draw(UIContext& ui) {
        drawInterfaceBackground(ui);
    }

    void drawInterfaceBackground(UIContext& ui) {
        Rect rect;
        rect.x = Config::SCREEN_WIDTH - Config::UI_PANEL_WIDTH;
        rect.y = 0.0f;
        rect.w = 300.0f;
        rect.h = Config::SCREEN_HEIGHT;

        glm::vec4 color = {0.1f, 0.1f, 0.1f, 1.0f};

        ui.drawRect("white", rect, color);
    }
};

class HotbarUI {
public:
    void draw(UIContext& ui, PlayerInventory& inventory) {
        drawHotbar(ui);
        drawIcons(ui, inventory);
    }

    void drawHotbar(UIContext& ui) {
        float slotStride    = iconSize_ + border_;                           // 17 — distance from one slot's start to the next
        float totalWidth    = border_ + slotCount_ * slotStride;             // 1 + 4*17 = 69
        float totalHeight   = iconSize_ + border_ * 2.0f;                    // 18

        Rect square;
        square.x = hotbarX_;
        square.y = hotbarY_;
        square.w = totalWidth * scale_;
        square.h = totalHeight * scale_;

        ClickResult result = ui.ImageArea("hotbar", square);
    }

    void drawIcons(UIContext& ui, PlayerInventory& inventory) {
        float slotStride = iconSize_ + border_;   // must match drawHotbar's stride exactly

        for (int i = 0; i < slotCount_; i++) {
            std::string item = inventory.getHotbarItem(i);
            if (item == "empty") continue;   // nothing to draw — hotbar background shows through

            float x = hotbarX_ + scale_ * (border_ + i * slotStride);
            float y = hotbarY_ + scale_ * border_;

            Rect square{x, y, iconSize_ * scale_, iconSize_ * scale_};
            ui.drawRect(item.c_str(), square);
        }
    }

private:
    float hotbarX_ = Config::SCREEN_WIDTH - 250.0f;
    float hotbarY_ = 400.0f;

    float iconSize_ = 16.0f;
    float border_   = 1.0f;

    const int slotCount_ = 4;
    const float scale_ = 3.0f;
};