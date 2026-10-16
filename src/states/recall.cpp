#include <cambc.h>
#include "recall.h"
#include "farm.h"
#include "shop.h"
#include "../core/context.h"
#include "../core/match_data.h"
#include "../nav/pathfinding.h"
#include "../constants/brain_constants.h"
#include "../constants/state_constants.h"
namespace {
bool on_spawn() { return cambc::map::spawn_region(cambc::team::ally).is_set(cambc::self().position()); }
}
// Heal: when badly hurt, recall; a hurt champion on spawn stays there for the turn, since
// spawn heals only if the turn ends there.
int RecallState::score(const TurnContext& ctx) {
    auto& me = cambc::self();
    if (on_spawn()) return me.health() < me.max_health() ? constants::kRecallScore : 0;
    if (me.health() < me.max_health() * constants::kRecallHpFraction) return constants::kRecallScore;
    // Hurt with nothing to farm: go heal instead of standing around.
    if (me.health() < me.max_health() && !farm_target(*ctx.match_data)) return constants::kRecallScore;
    return 0;
}
StateResult RecallState::execute(TurnContext& ctx, BotMemory& memory) {
    auto& me = cambc::self();
    if (on_spawn()) {
        shop_pre_step();
        return StateResult::Continue;
    }
    // Damage cancels a recall, so only start one if nothing hit us since our last turn;
    // otherwise walk home (which also gets us out of camp range).
    if (!ctx.match_data->hit_since_last_turn) {
        me.recall();
        return StateResult::Done;
    }
    move_towards_closest(cambc::map::spawn_region(cambc::team::ally));
    return StateResult::Continue;
}
RecallState recall_state_instance;
