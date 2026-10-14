#include <cambc.h>
#include "../core/match_data.h"
using cambc::vec2;
bool stage_map_snapshot(MatchData& match_data) {
    for (const auto& group : cambc::map::camp_groups()) {
        CachedCampGroup cached;
        cached.handle = group;
        cached.spawn_tiles = group.spawn_tiles();
        cached.gold_value = group.gold_value();
        cached.gem_value = group.gem_value();
        cached.max_respawn_timer = group.max_respawn_timer();
        match_data.camp_groups.push_back(cached);
    }
    match_data.static_blockers = cambc::map::walls() | cambc::map::holes()
        | cambc::map::altars() | cambc::map::spawn_region(cambc::team::enemy);
    return true;
}
