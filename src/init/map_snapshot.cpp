#include <cambc.h>
#include <utility>
#include <algorithm>
#include <vector>
#include "../constants/test_hooks.h"
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
        if constexpr (SEQ_CAMP_DAMAGE) {
            std::vector<std::pair<float, float>> camps;   // (health, strength), weakest first
            for (const auto& camp : group) camps.emplace_back(camp.base_stats().base_health, camp.base_stats().base_strength);
            std::sort(camps.begin(), camps.end());
            float remaining = cached.design_strength;
            for (const auto& [hp, str] : camps) { cached.seq_damage_unit += remaining * hp; remaining -= str; }
        }
        cached.view = std::move(group);
        match_data.camp_groups.push_back(std::move(cached));
    }
    // Distance from each group to both spawns (constants::kEnemySidePlan).
    if constexpr (ENEMY_SIDE_PLAN > 0) {
        const auto own = cambc::map::spawn_region(cambc::team::ally).positions();
        const auto enemy = cambc::map::spawn_region(cambc::team::enemy).positions();
        const auto nearest = [](cambc::vec2 c, const std::vector<cambc::vec2>& tiles) {
            int best = 1 << 20;
            for (const auto t : tiles) best = std::min(best, static_cast<int>(c.chebyshev_distance(t)));
            return best;
        };
        for (auto& g : match_data.camp_groups) {
            g.own_spawn_dist = nearest(g.centre, own);
            g.enemy_spawn_dist = nearest(g.centre, enemy);
        }
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
