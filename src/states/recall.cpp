#include <cambc.h>
#include <iostream>
#include "../constants/test_hooks.h"
#include "recall.h"
#include "farm.h"
#include "shop.h"
#include "attack.h"
#include "tribute.h"
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

// Shop trip (constants::kShopRecall): the gold we carry finishes our next planned emblem and we are
// between jobs. Cheapest checks first: most turns stop at the gold check.
bool shop_trip_due(const TurnContext& ctx) {
    auto& me = cambc::self();
    if (me.passive() == cambc::passive_type::remote_shop) return false;   // shops anywhere already
    const auto need = next_emblem_gold(me);
    if (!need || me.gold_count() < *need) return false;
    auto& match = *ctx.match_data;
    if (match.hit_since_last_turn) return false;
    if (const auto target = farm_target(match)) {
        const auto& group = match.camp_groups[static_cast<std::size_t>(*target)];
        // Capturing, or about to: a waiting reward next to us.
        if (group.reward_waiting) {
            int nearest = 1 << 20;
            for (const auto tile : group.spawn_positions) nearest = std::min(nearest, distance(tile, me.position()));
            if (nearest <= constants::kCollectorRadius) return false;
        }
        // Hitting it.
        for (const auto& camp : *group.view)
            if (camp.is_visible() && camp.health() > 0 && me.attack_tiles().is_set(camp.position())) return false;
    }
    if (tribute_state_instance.score(ctx) > 0) return false;   // bank the gems first
    if (attack_state_instance.score(ctx) > 0) return false;    // an enemy in reach
    return true;
}
}
// Heal: when badly hurt, recall; a hurt champion on spawn stays there for the turn, since
// spawn heals only if the turn ends there.
int RecallState::score(const TurnContext& ctx) {
    auto& me = cambc::self();
    if (on_spawn()) return me.health() < me.max_health() ? constants::kRecallScore : 0;
    if (me.health() < me.max_health() * constants::kRecallHpFraction) return constants::kRecallScore;
    // Hurt with nothing to farm: go heal instead of standing around.
    if (me.health() < me.max_health() && !farm_target(*ctx.match_data)) return constants::kRecallScore;
    // Gold for our next emblem and nothing else going on: go shopping.
    if (constants::kShopRecall && shop_trip_due(ctx)) return constants::kShopRecallScore;
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
