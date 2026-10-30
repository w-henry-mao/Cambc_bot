#pragma once
#include "../nav/pathfinding.h"
#include <cstdint>
#include <limits>

struct BotMemory {
    nav::MovementMemory movement;
    bool recall_channel_started = false;
    std::uint32_t spawn_arrival_round = std::numeric_limits<std::uint32_t>::max();
};
