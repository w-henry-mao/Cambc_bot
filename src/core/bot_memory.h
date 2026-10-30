#pragma once
#include "../nav/pathfinding.h"
#include <cstdint>
#include <limits>
#include <optional>

struct BotMemory {
    nav::MovementMemory movement;
    bool recall_channel_started = false;
    std::uint32_t spawn_arrival_round = std::numeric_limits<std::uint32_t>::max();
    std::optional<cambc::handle_champion_t> pursuit_target;
    std::optional<cambc::handle_champion_t> abandoned_pursuit_target;
    std::uint32_t pursuit_last_damage_round = 0;
};
