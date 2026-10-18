#include "wards.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <cambc.h>
#include "../constants/test_hooks.h"
#include "../constants/state_constants.h"
#include "../core/match_data.h"
// Goal tile for our ward: 1 map centre, 2 our farm camp (centre without one), 3 a point
// kWardAheadDist tiles from us toward the enemy spawn.
namespace {
cambc::vec2 ward_goal(const MatchData& match) {
    auto& me = cambc::self();
    const cambc::vec2 centre{cambc::map::width() / 2, cambc::map::height() / 2};
    if constexpr (constants::kWardPlan == 2) {
        if (match.farm_target) return match.camp_groups[static_cast<std::size_t>(*match.farm_target)].centre;
        return centre;
    } else if constexpr (constants::kWardPlan == 3) {
        static std::optional<cambc::vec2> enemy_spawn;
        if (!enemy_spawn) {
            const auto tiles = cambc::map::spawn_region(cambc::team::enemy).positions();
            long sx = 0, sy = 0;
            for (const auto t : tiles) { sx += t.x; sy += t.y; }
            enemy_spawn = tiles.empty() ? centre : cambc::vec2{static_cast<int>(sx / static_cast<long>(tiles.size())), static_cast<int>(sy / static_cast<long>(tiles.size()))};
        }
        const auto p = me.position();
        const int dx = enemy_spawn->x - p.x, dy = enemy_spawn->y - p.y;
        const int d = std::max(std::abs(dx), std::abs(dy));
        if (d <= constants::kWardAheadDist) return *enemy_spawn;
        return {p.x + dx * constants::kWardAheadDist / d, p.y + dy * constants::kWardAheadDist / d};
    }
    return centre;
}
}
void ward_step(const MatchData& match) {
    if constexpr (constants::kWardPlan == 0) return;
    auto& me = cambc::self();
    if (!me.is_alive() || me.ward_cooldown() > 0) return;
    const auto here = me.position();
    const auto mine = me.ward_positions();
    const auto goal = ward_goal(match);
    // Keep a ward we are still near; otherwise place one on the reachable tile closest to the goal.
    if (!mine.empty() && mine.front().chebyshev_distance(here) <= static_cast<std::uint32_t>(constants::kWardReplaceDist)) return;
    const auto tiles = (me.ward_placement_tiles() & ~cambc::vision::ward_tiles(cambc::team::ally)).positions();
    std::optional<cambc::vec2> best;
    std::uint32_t best_d = 0;
    for (const auto t : tiles) {
        const auto d = t.chebyshev_distance(goal);
        if (!best || d < best_d) { best = t; best_d = d; }
    }
    if (!best) return;
    try {
        const bool ok = me.place_ward(*best);
        if constexpr (STATE_TRACE) std::cout << "WARD " << cambc::round_number() << ' ' << best->x << ' ' << best->y << ' ' << ok << '\n';
    } catch (const cambc::error& e) {
        if constexpr (STATE_TRACE) std::cout << "WARD_FAIL " << cambc::round_number() << ' ' << e.what() << '\n';
    }
}
