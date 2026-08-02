#pragma once
#include "coah_engine/ui_context.hpp"

class HotbarUI {
public:
    HotbarUI() {}

    void draw(UIContext& ui) {
        Rect square;
        square.x = 20.0f;
        square.y = 20.0f;
        square.w = 40.0f;
        square.h = 40.0f;

        UVRect uvCoords;
        ClickResult imageArea = ui.ImageArea("Hotbar", square, uvCoords);
    }

private:
};