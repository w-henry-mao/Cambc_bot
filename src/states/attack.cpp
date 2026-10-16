#include <cambc.h>
#include "attack.h"
#include "../core/context.h"
#include "../core/geometry.h"
#include "../core/roster.h"
#include "../skills/casting.h"
#include "../constants/brain_constants.h"
namespace {
// Enemy champions we can hit this turn without walking: basic attack or ability range.
int reach() {
    auto& me = cambc::self();
    int r = me.attack_range();
    for (const auto& ab : me.abilities()) {
        if (ab.is_damage()) r = std::max(r, static_cast<int>(ab.get<cambc::ability_damage>().range));
        else if (ab.is_cc()) r = std::max(r, static_cast<int>(ab.get<cambc::ability_cc>().range));
        else if (ab.is_aoe_damage()) r = std::max(r, static_cast<int>(ab.get<cambc::ability_aoe_damage>().range));
    }
    return r;
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
        // In ability range but not attack range: close in, then attack if we now can.
        me.move_towards(target->position());
        if (target->is_visible() && target->health() > 0) attack_with_reset(*target);
    }
    return StateResult::Continue;
}
AttackState attack_state_instance;
