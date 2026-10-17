#include <cambc.h>
#include <iostream>
#include "../constants/test_hooks.h"
#include "recall.h"
#include "farm.h"
#include "shop.h"
#include "../core/context.h"
#include "../core/match_data.h"
#include "../nav/pathfinding.h"
#include "../nav/safe_recall.h"
#include "../skills/dash_move.h"
#include "../core/geometry.h"
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
    const bool hit = ctx.match_data->hit_since_last_turn;
    if constexpr (constants::kSafeRecall) {
        // Damage cancels a recall: first get out of reach of every enemy champion and awake camp
        // we can see (nav/safe_recall.h), then recall. Hit since last turn by something we cannot
        // see, or no safe tile in range: walk home as before.
        const auto f0 = cambc::fuel_remaining();
        const auto plan = plan_safe_recall(*ctx.match_data);
        if constexpr (RECALL_TRACE)
            std::cout << "RECALL_PLAN fuel=" << (f0 - cambc::fuel_remaining()) << " safe=" << plan.safe_here << " spot=" << plan.spot.has_value()
                      << " threats=" << plan.threats << '\n';
        recall_trace_note_start(plan.spot.has_value(), plan.threats);
        if (plan.safe_here && !hit) {
            me.recall();
            return StateResult::Done;
        }
        if (!plan.safe_here && plan.spot && me.can_move()) {
            if constexpr ((constants::kDashPlan & 2) != 0) dash_towards(tiles_around(*plan.spot, 0));
            if (me.position() != *plan.spot) move_towards_closest(tiles_around(*plan.spot, 0));
            if (!hit && !plan.danger.is_set(me.position())) {
                try {
                    me.recall();
                    if constexpr (RECALL_TRACE) std::cout << "RECALL_AFTER_MOVE ok\n";
                    return StateResult::Done;
                } catch (const cambc::error& e) {
                    if constexpr (RECALL_TRACE) std::cout << "RECALL_AFTER_MOVE failed " << e.what() << '\n';
                }
            }
            return StateResult::Continue;
        }
    } else {
        recall_trace_note_start(false, 0);
        // Damage cancels a recall, so only start one if nothing hit us since our last turn;
        // otherwise walk home (which also gets us out of camp range).
        if (!hit) {
            me.recall();
            return StateResult::Done;
        }
    }
    move_towards_closest(cambc::map::spawn_region(cambc::team::ally));
    return StateResult::Continue;
}
RecallState recall_state_instance;
