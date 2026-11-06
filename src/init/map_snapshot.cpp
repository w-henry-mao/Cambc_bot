#include "stages.h"
#include "../constants/tunable_constants.h"
#include <cambc.h>

void stage_map_snapshot(MatchData& match_data) {
    constexpr std::int32_t enemy_spawn_avoidance_radius = tunable_constants::kEnemySpawnAvoidanceRadius;
    const auto enemy_spawn = cambc::map::spawn_region(cambc::team::enemy);
    for (const auto spawn : enemy_spawn.positions()) {
        for (std::int32_t dy = -enemy_spawn_avoidance_radius; dy <= enemy_spawn_avoidance_radius; ++dy) {
            for (std::int32_t dx = -enemy_spawn_avoidance_radius; dx <= enemy_spawn_avoidance_radius; ++dx) {
                const cambc::vec2 tile{spawn.x + dx, spawn.y + dy};
                if (cambc::map::is_in_bounds(tile)) match_data.enemy_spawn_avoidance_zone.set(tile);
            }
        }
    }
    match_data.ally_spawn_positions = cambc::map::spawn_region(cambc::team::ally).positions();

    match_data.camp_groups.clear();
    auto groups = cambc::map::camp_groups();
    match_data.camp_groups.reserve(groups.size());

    for (auto& group : groups) {
        CachedCampGroup cached;
        cached.gold_value = group.gold_value();
        cached.gem_value = group.gem_value();

        for (const auto& camp : group) {
            const auto stats = camp.base_stats();
            cached.design_health += stats.base_health;
            if (stats.base_attack_range > 0) {
                cached.camp_threats.push_back({camp.spawn_position(), stats.base_attack_range, true});
            }
        }

        cached.view = std::move(group);
        match_data.camp_groups.push_back(std::move(cached));
    }

    match_data.static_blockers = cambc::map::walls() | cambc::map::holes()
        | cambc::map::altars() | cambc::map::spawn_region(cambc::team::enemy);
}
