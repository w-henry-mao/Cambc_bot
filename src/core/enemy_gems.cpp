#include "enemy_gems.h"
#include "roster.h"
#include "../combat/design_table.h"
#include "../combat/our_policy.h"

namespace {
struct Last { std::uint32_t gems = 0, round = 0; bool seen = false; };
Last last[8];

int roster_slot(const cambc::champion& c) {
    const auto& enemies = roster_enemies();
    for (std::size_t i = 0; i < enemies.size() && i < 8; ++i)
        if (enemies[i] == c) return static_cast<int>(i);
    return -1;
}
}  // namespace

EnemyGems enemy_gems(const cambc::champion& enemy, const MatchData& match) {
    EnemyGems out;
    const int slot = roster_slot(enemy);
    const auto round = cambc::round_number();
    if (!in_view(enemy)) {
        if (slot >= 0 && last[slot].seen) {
            out.known = true;
            out.carried = last[slot].gems;
            out.rounds_old = round - last[slot].round;
        }
        return out;
    }
    out.known = true;
    out.carried = enemy.gem_count();
    if (slot >= 0) last[slot] = Last{out.carried, round, true};
    // The camp group it is fighting: a damaged camp within its reach (the camp's group is found by
    // its spawn tile among the groups cached at match start).
    const auto p = enemy.position();
    const int r = threat_reach(design_for(static_cast<int>(enemy.champion_name())));
    for (const auto& c : cambc::vision::camps()) {
        if (c.health() <= 0 || c.health() >= c.max_health() || static_cast<int>(p.chebyshev_distance(c.position())) > r) continue;
        const auto spawn = c.spawn_position();
        for (const auto& g : match.camp_groups)
            if (g.spawn_tiles.is_set(spawn)) { out.camp_gems = g.gem_value; return out; }
    }
    return out;
}
