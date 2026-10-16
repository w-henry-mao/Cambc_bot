#include <cambc.h>
#include "../core/geometry.h"
#include "../core/match_data.h"
#include "stages.h"
bool stage_map_snapshot(MatchData& match_data) {
    match_data.camp_groups.clear();
    for (const auto& group : cambc::map::camp_groups()) {
        CachedCampGroup cached;
        cached.handle = group;
        cached.view = group;
        cached.spawn_tiles = group.spawn_tiles();
        cached.spawn_positions = cached.spawn_tiles.positions();
        cached.centre = cached.spawn_positions[cached.spawn_positions.size() / 2];
        cached.gold_value = group.gold_value();
        cached.gem_value = group.gem_value();
        for (const auto& camp : group) {
            const auto stats = camp.base_stats();
            cached.design_health += stats.base_health;
            cached.design_strength += stats.base_strength;
        }
        match_data.camp_groups.push_back(cached);
    }
    match_data.static_blockers = cambc::map::walls() | cambc::map::holes()
        | cambc::map::altars() | cambc::map::spawn_region(cambc::team::enemy);
    // Altars never move, so the walk to the nearest one is computed once for the whole match.
    cambc::bitboard approach;
    approach.clear();
    for (const auto altar : cambc::map::altars().positions()) approach |= tiles_around(altar, cambc::constants::submit_gems_range);
    approach &= ~match_data.static_blockers;
    cambc::bitboard visited;
    match_data.altar_distance.compute(match_data.static_blockers, approach, visited);
    return true;
}
