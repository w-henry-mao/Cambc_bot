#pragma once
#include <cambc.h>
#include <vector>
struct CachedCampGroup {
    cambc::handle_camp_group_t handle;
    cambc::bitboard spawn_tiles;
    std::uint32_t gold_value;
    std::uint32_t gem_value;
    std::int32_t max_respawn_timer;
};
struct MatchData {
    std::vector<CachedCampGroup> camp_groups;
    cambc::bitboard static_blockers;
    bool initialized = false;
};
