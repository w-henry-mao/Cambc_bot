#include "stages.h"
#include <cambc.h>

void stage_map_snapshot(MatchData& match_data) {
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
