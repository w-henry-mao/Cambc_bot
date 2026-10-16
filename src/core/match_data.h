#pragma once
#include <cambc.h>
#include <optional>
#include <vector>
// What we know about a camp group. Static parts are filled once by the map snapshot; the rest
// is updated while the group is in sight.
struct CachedCampGroup {
    cambc::handle_camp_group_t handle;
    std::optional<cambc::map::camp_group> view;   // live queries go through this
    cambc::bitboard spawn_tiles;
    std::vector<cambc::vec2> spawn_positions;
    cambc::vec2 centre{0, 0};                      // a spawn tile in the middle of the group
    std::uint32_t gold_value = 0;
    std::uint32_t gem_value = 0;
    float design_health = 0;                       // all its camps at full health
    float design_strength = 0;                     // their combined damage per round
    std::uint32_t respawn_ready_round = 0;         // last seen respawn timer as an absolute round
    bool reward_waiting = false;                   // last seen dead with the reward not captured
};
struct MatchData {
    std::vector<CachedCampGroup> camp_groups;
    cambc::bitboard static_blockers;               // walls, holes, altars, enemy spawn
    cambc::distance_field altar_distance;          // walking distance to the nearest altar's tribute range
    bool initialized = false;
    float last_health = -1;                        // our health at the end of our previous turn
    bool hit_since_last_turn = false;
    std::optional<int> farm_target;                // camp group we are working on
};
