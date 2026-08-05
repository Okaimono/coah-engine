#pragma once

namespace Config {
    inline constexpr int SCREEN_WIDTH  = 1200;
    inline constexpr int SCREEN_HEIGHT = 600;
    inline constexpr int UI_PANEL_WIDTH = 300;
    inline constexpr int GAME_WIDTH  = SCREEN_WIDTH - UI_PANEL_WIDTH;   // 1100
    inline constexpr int GAME_HEIGHT = SCREEN_HEIGHT;                   // 600
}