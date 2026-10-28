#pragma once
#include <cstdint>

struct DecodedDraftMemory {
    int altar_mode = 2;  // 0=Single, 1=Zone, 2=Normal
    int resource_mode = 2;  // 0=GemsOnly, 1=CampsOnly, 2=Normal
    bool combative = true;
    int altar_position_x = -1;  // -1 if none
    int altar_position_y = -1;  // -1 if none
};
