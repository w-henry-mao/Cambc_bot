#pragma once
#include <cambc.h>
#include <algorithm>
#include <optional>
#include <cstdint>
#include <vector>
// What we know about a camp group. Static parts are filled once by the map snapshot; the rest
// is updated while the group is in sight.
struct CachedCampGroup {
    cambc::handle_camp_group_t handle;
    std::optional<cambc::map::camp_group> view;   // live queries go through this
    cambc::bitboard spawn_tiles;
    cambc::bitboard beside_tiles;                  // open tiles next to its spawn tiles, none of them a spawn tile of any group
    std::vector<cambc::vec2> spawn_positions;
    cambc::vec2 centre{0, 0};                      // a spawn tile in the middle of the group
    std::uint32_t gold_value = 0;
    std::uint32_t gem_value = 0;
    float design_health = 0;                       // all its camps at full health
    float design_strength = 0;                     // their combined damage per round
    std::uint32_t respawn_ready_round = 0;         // last seen respawn timer as an absolute round
    bool reward_waiting = false;                   // last seen dead with the reward not captured
    std::uint32_t max_respawn = 0;                 // configured respawn timer
    std::uint32_t left_to_teammate_round = 0;      // round we left its reward to a teammate (0: not left)
};
// Round the group is ready again: the last seen respawn timer, or after we left its reward to a
// teammate, when that capture should finish plus the respawn timer.
inline std::uint32_t respawn_ready(const CachedCampGroup& group) {
    if (!group.left_to_teammate_round) return group.respawn_ready_round;
    const auto assumed = group.left_to_teammate_round + static_cast<std::uint32_t>(cambc::constants::capture_time) + group.max_respawn;
    return std::max(group.respawn_ready_round, assumed);
}
struct MatchData {
    std::vector<CachedCampGroup> camp_groups;
    cambc::bitboard static_blockers;               // walls, holes, altars, enemy spawn
    cambc::distance_field altar_distance;          // walking distance to the nearest altar's tribute range
    // Walking distance to our spawn region and bush tiles as plain row-major arrays (index
    // y * map_width + x): reading them costs no host calls, unlike distance_field::get/is_set.
    std::vector<std::uint16_t> home_walk;
    std::vector<std::uint8_t> bush_tiles;
    int map_width = 0;
    cambc::bitboard sample_grid;                   // tiles with even x and y (safe recall scores these)
    bool initialized = false;
    float last_health = -1;                        // our health at the end of our previous turn
    bool hit_since_last_turn = false;
    std::optional<int> farm_target;                // camp group we are working on
};
