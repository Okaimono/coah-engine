#pragma once

#include <string>
#include <vector>

struct ItemSlot {
    std::string item;
};

class PlayerInventory {
public:
    PlayerInventory() {
        for (int i = 0; i < 4; i++) {
            hotbar_[i].item = "none";
        }
        hotbar_[0].item = "morning_star";
    }

    std::string getHotbarItem(int slot) {
        if (slot >= hotbarSize) { return "white"; }
        return hotbar_[slot].item;
    }

private:
    int hotbarSize = 4;
    ItemSlot hotbar_[4]; 
};