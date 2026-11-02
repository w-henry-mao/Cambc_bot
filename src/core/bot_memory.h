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
    std::uint32_t last_carried_gems = 0;
    std::uint32_t last_gem_increase_round = 0;
    bool gem_progress_initialized = false;
    bool tribute_eligible = false;
    std::uint32_t carried_gems = 0;
    std::optional<cambc::vec2> tribute_altar;
    bool final_tribute_roster_initialized = false;
    std::uint8_t final_tribute_slot = 0;
    std::uint8_t final_tribute_roster_mask = 0;
    bool final_tribute_death_round_initialized = false;
    std::uint32_t final_tribute_death_round = 0;
    std::uint32_t final_tribute_resume_round = 0;
    std::uint32_t prism_alone_rounds = 0;
    std::uint32_t prism_alone_last_round = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t prism_dead_since_round = std::numeric_limits<std::uint32_t>::max();
    bool prism_regroup_active = false;
    std::optional<cambc::vec2> prism_farm_target;
    std::uint32_t prism_stall_rounds = 0;
    std::uint32_t prism_stall_last_round = std::numeric_limits<std::uint32_t>::max();
    std::optional<cambc::vec2> prism_stall_sample_position;
    std::uint32_t prism_stall_sample_round = std::numeric_limits<std::uint32_t>::max();
    bool prism_stall_sample_had_move_attempt = false;
    bool prism_tether_released = false;
    std::uint32_t prism_release_move_rounds = 0;
    std::uint32_t prism_release_last_move_round = std::numeric_limits<std::uint32_t>::max();
};
