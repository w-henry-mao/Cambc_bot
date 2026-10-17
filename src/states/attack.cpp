#include <cambc.h>
#include "attack.h"
#include "../core/context.h"
#include "../core/geometry.h"
#include "../core/roster.h"
#include "../skills/casting.h"
#include "../combat/sdk_team_fight.h"
#include "../nav/pathfinding.h"
#include "../constants/brain_constants.h"
#include "../constants/test_hooks.h"
#include "../constants/combat_constants.h"
#include <iostream>
namespace {
// Enemy champions we can hit this turn without walking: basic attack or ability range, read live
// so game patches are picked up. The fight simulator's copy (combat/our_policy.h reach()) uses
// the generated design table; tools/duel_harness.py checks the two fight alike.
int reach() {
    auto& me = cambc::self();
    int r = me.attack_range();
    for (const auto& ab : me.abilities()) {
        if (ab.is_damage()) r = std::max(r, static_cast<int>(ab.get<cambc::ability_damage>().range));
        else if (ab.is_cc()) r = std::max(r, static_cast<int>(ab.get<cambc::ability_cc>().range));
        else if (ab.is_aoe_damage()) r = std::max(r, static_cast<int>(ab.get<cambc::ability_aoe_damage>().range));
    }
    return r + REACH_BONUS;   // test builds may widen engagement (constants/test_hooks.h); 0 normally
}
const cambc::champion* weakest_enemy_within(int radius) {
    const auto me = cambc::self().position();
    const cambc::champion* best = nullptr;
    for (const auto& e : roster_enemies())
        if (in_view(e) && distance(e.position(), me) <= radius && (!best || e.health() < best->health())) best = &e;
    return best;
}
}
// Fight back whenever an enemy champion is within reach: the weakest one first. While a teammate's
// fight call is live (combat/sdk_team_fight.h, brain/team_fight_call.cpp), the called enemy comes
// first whenever it is within reach, and with nobody in reach we walk to it.
int AttackState::score(const TurnContext& ctx) {
    if (weakest_enemy_within(reach())) return constants::kAttackScore;
    if (team_fight_called()) return constants::kTeamFightCallScore;
    return 0;
}

namespace {
// Walk toward the tiles within attack range of `spot`.
void walk_to(vec2 spot) { move_towards_closest(tiles_around(spot, cambc::self().attack_range())); }
}

StateResult AttackState::execute(TurnContext& ctx, BotMemory& memory) {
    auto& me = cambc::self();
    const int r = reach();
    const auto* target = weakest_enemy_within(r);
    if (const auto order = team_fight_called()) {
        const auto& called = *order->target;
        const bool called_visible = in_view(called);
        if constexpr (constants::kTeamFightTestCaller)
            std::cout << "TEAM_FOLLOW r=" << cambc::round_number() << " me=" << cambc::to_string(me.champion_name())
                      << " target=" << (called_visible ? cambc::to_string(called.champion_name()) : "?")   // hidden: reading it throws
                      << " visible=" << called_visible
                      << " dist=" << distance(called_visible ? called.position() : order->last_seen, me.position())
                      << " other_in_reach=" << (target != nullptr) << '\n';
        if (called_visible && distance(called.position(), me.position()) <= r) {
            target = &called;
        } else if (!target) {
            // Nobody in reach: answer the call. Head for the called enemy (its live position when we
            // can see it, otherwise where the caller last saw it).
            cast_support();
            if (me.can_move()) walk_to(called_visible ? called.position() : order->last_seen);
            return StateResult::Continue;
        }
    }
    if (!target) return StateResult::Done;
    cast_support();
    cast_at(*target);
    if (!attack_with_reset(*target) && me.can_move() && target->is_visible()) {
        // In ability range but not attack range: close in, then attack if we now can. Walk toward
        // the free tiles within attack range of the target: its own tile is occupied, so
        // move_towards(target position) never moves.
        walk_to(target->position());
        if (target->is_visible() && target->health() > 0) attack_with_reset(*target);
    }
    return StateResult::Continue;
}
AttackState attack_state_instance;
