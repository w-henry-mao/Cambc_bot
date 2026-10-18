#include <cambc.h>
#include <utility>
#include "../core/geometry.h"
#include "../core/match_data.h"
#include "stages.h"
// What play needs from its first turn: the camp groups, the static blockers and the walk to the
// altars. The rest of the setup is done at the end of turns (init/deferred_setup.h).
bool stage_map_snapshot(MatchData& match_data) {
    match_data.camp_groups.clear();
    auto groups = cambc::map::camp_groups();
    // Reserved and moved in: copying a group copies its camps and bitboards (~1.3k fuel each, and
    // a growing vector copies every earlier group again).
    match_data.camp_groups.reserve(groups.size());
    for (auto& group : groups) {
        CachedCampGroup cached;
        cached.handle = group;
        cached.spawn_tiles = group.spawn_tiles();
        cached.spawn_positions = cached.spawn_tiles.positions();
        cached.centre = cached.spawn_positions[cached.spawn_positions.size() / 2];
        cached.gold_value = group.gold_value();
        cached.gem_value = group.gem_value();
        cached.max_respawn = static_cast<std::uint32_t>(std::max(0, group.max_respawn_timer()));
        for (const auto& camp : group) {
            const auto stats = camp.base_stats();
            cached.design_health += stats.base_health;
            cached.design_strength += stats.base_strength;
        }
        cached.view = std::move(group);
        match_data.camp_groups.push_back(std::move(cached));
    }
    match_data.static_blockers = cambc::map::walls() | cambc::map::holes()
        | cambc::map::altars() | cambc::map::spawn_region(cambc::team::enemy);
    match_data.map_width = cambc::map::width();
    // Altars never move, so the walk to the nearest one is computed once for the whole match.
    cambc::bitboard approach;
    approach.clear();
    for (const auto altar : cambc::map::altars().positions()) approach |= tiles_around(altar, cambc::constants::submit_gems_range);
    approach &= ~match_data.static_blockers;
    cambc::bitboard visited;
    match_data.altar_distance.compute(match_data.static_blockers, approach, visited);
    return true;
}
