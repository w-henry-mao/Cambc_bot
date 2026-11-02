#pragma once
#include <cambc.h>
#include <cstddef>
#include <optional>

namespace nav {
inline constexpr std::size_t kDashSlot = 1;

struct PathScratch {
    cambc::bitboard landing_blocked;
    cambc::bitboard route_blocked;
    cambc::bitboard goals;
    cambc::bitboard visited;
    cambc::bitboard ally_tiles;
    cambc::bitboard enemy_tiles;
    cambc::bitboard camp_tiles;
    cambc::bitboard teleport_tiles;
    cambc::distance_field goal_field;
};

struct MovementMemory {
    bool initialized = false;
    bool has_dash = false;
    bool is_heart_of_ruin = false;
    bool resets_cooldowns_on_kill = false;
    int cooldown = 0;
    std::optional<cambc::ability> ability;
    std::optional<cambc::vec2> movement_guard_center;
    int movement_guard_radius = 0;
    PathScratch scratch;
};

// Route all farm movement here. Uses a dash when it improves travel, otherwise walks.
// A forbidden mask constrains both walking and dash landing tiles.
bool move_towards(MovementMemory& movement, cambc::vec2 target, bool allow_dash = true,
                  bool target_is_camp = false,
                  const cambc::bitboard* additional_forbidden_tiles = nullptr);
bool move_into_radius(MovementMemory& movement, cambc::vec2 center, int radius,
                      bool fallback_to_closest = false,
                      const cambc::bitboard* additional_forbidden_tiles = nullptr);
bool move_randomly_within_radius(MovementMemory& movement, cambc::vec2 center, int radius,
                                 const cambc::bitboard& forbidden_tiles);
bool move_off(MovementMemory& movement, const cambc::bitboard& forbidden_tiles);
// Refresh visible ally and enemy champion occupancy masks for destination filtering.
void refresh_visible_champion_tiles(cambc::bitboard& ally_tiles, cambc::bitboard& enemy_tiles);
void filter_legal_dash_landings(MovementMemory& movement, cambc::vec2 start,
                                const cambc::bitboard& in_range, cambc::bitboard& legal);
}
