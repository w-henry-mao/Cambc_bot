#include "channel_safety.h"
#include <algorithm>
#include "../combat/design_table.h"
#include "../combat/our_policy.h"
#include "../constants/brain_constants.h"
#include "../core/roster.h"

namespace {
// Damage per turn a visible enemy deals: Strength, plus damage spells spread over their cooldown.
float damage_per_turn(const cambc::champion& e) {
    const auto& d = design_for(static_cast<int>(e.champion_name()));
    float dmg = e.strength();
    for (const auto& ab : d.slots)
        if (ab.kind == AbilityKind::Damage || ab.kind == AbilityKind::AoeDamage || ab.kind == AbilityKind::Cc)
            dmg += ab.modifier * e.focus() / static_cast<float>(std::max(1, ab.cooldown));
    return dmg;
}
}  // namespace

bool channel_still_safe(const MatchData& match) {
    auto& me = cambc::self();
    // Damage already cancels a recall (engine), so recalls are left alone.
    if (!me.is_submitting_gems()) return true;
    switch (constants::kTributeSafetyPlan) {
        case 1:   // hit since our last turn
            return !(match.last_health >= 0 && me.health() < match.last_health);
        case 2: {   // enemies already in reach could take our health before the tribute completes
            const int turns_left = std::max(1, me.remaining_channel_time() + 1);
            float danger = 0;
            const auto here = me.position();
            for (const auto& e : roster_enemies()) {
                if (!in_view(e)) continue;
                const int reach_e = threat_reach(design_for(static_cast<int>(e.champion_name())));
                if (static_cast<int>(e.position().chebyshev_distance(here)) <= reach_e) danger += damage_per_turn(e);
            }
            return danger * static_cast<float>(turns_left) < me.health();
        }
        default:
            return true;
    }
}
