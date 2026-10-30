#pragma once
#include <cambc.h>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>
// What we know about a camp group. Static parts are filled once by the map snapshot; the rest
// is updated while the group is in sight.
struct CachedCampGroup {
    std::optional<cambc::map::camp_group> view;   // live queries go through this
    cambc::bitboard spawn_tiles;
    bool spawn_ready = false;
    cambc::bitboard beside_tiles;                  // open tiles next to its spawn tiles, none of them a spawn tile of any group
    bool beside_ready = false;                     // beside tiles have been cached
    std::vector<cambc::vec2> spawn_positions;
    std::vector<cambc::vec2> beside_positions;
    cambc::bitboard harvest_near_tiles;
    bool harvest_near_ready = false;
    cambc::vec2 centre{0, 0};                      // a spawn tile in the middle of the group
    std::uint32_t gold_value = 0;
    std::uint32_t gem_value = 0;
    float design_health = 0;                       // all its camps at full health
    std::uint32_t respawn_ready_round = 0;         // last seen respawn timer as an absolute round
    std::uint32_t blocked_round = std::numeric_limits<std::uint32_t>::max();
    bool reward_waiting = false;
    std::uint32_t reward_seen_round = 0;
    std::uint32_t yield_until_round = 0;
    std::int32_t max_respawn = 0;
    bool max_respawn_cached = false;
};

inline std::uint32_t respawn_remaining(const CachedCampGroup& group, std::uint32_t now) {
    return group.respawn_ready_round > now ? group.respawn_ready_round - now : 0;
}
struct MatchData {
    std::vector<CachedCampGroup> camp_groups;
    cambc::bitboard static_blockers;               // walls, holes, altars, enemy spawn
    cambc::bitboard beside_mask;                   // not a spawn tile of any group, not a static blocker
    bool beside_mask_ready = false;
    bool map_snapshot_ready = false;
    bool initialized = false;
    std::uint32_t respawn_observed_round = std::numeric_limits<std::uint32_t>::max();
};
