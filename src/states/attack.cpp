#include <cambc.h>
#include "attack.h"
#include "../core/context.h"
#include "../core/geometry.h"
#include "../core/roster.h"
#include "../skills/casting.h"
#include "../nav/pathfinding.h"
#include "../constants/brain_constants.h"
#include "../constants/test_hooks.h"
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
// Fight back whenever an enemy champion is within reach: the weakest one first.
int AttackState::score(const TurnContext& ctx) {
    return weakest_enemy_within(reach()) ? constants::kAttackScore : 0;
}
StateResult AttackState::execute(TurnContext& ctx, BotMemory& memory) {
    const auto* target = weakest_enemy_within(reach());
    if (!target) return StateResult::Done;
    auto& me = cambc::self();
    cast_support();
    cast_at(*target);
    if (!attack_with_reset(*target) && me.can_move() && target->is_visible()) {
        // In ability range but not attack range: close in, then attack if we now can. Walk toward
        // the free tiles within attack range of the target: its own tile is occupied, so
        // move_towards(target position) never moves.
        move_towards_closest(tiles_around(target->position(), me.attack_range()));
        if (target->is_visible() && target->health() > 0) attack_with_reset(*target);
    }
    return StateResult::Continue;
}
AttackState attack_state_instance;
