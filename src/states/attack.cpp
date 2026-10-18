#include <cambc.h>
#include "attack.h"
#include "../core/context.h"
#include "../core/geometry.h"
#include "../core/roster.h"
#include "../core/match_data.h"
#include "../combat/design_table.h"
#include "../combat/our_policy.h"
#include <optional>
#include <vector>
#include "../skills/casting.h"
#include "../skills/dash_move.h"
#include "../combat/sdk_team_fight.h"
#include "../nav/pathfinding.h"
#include "../constants/brain_constants.h"
#include "../constants/state_constants.h"
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
// An enemy busy with a camp: the nearest awake (damaged) camp within its reach, how far the enemy
// stands from it, and how much health that camp group has left (fraction of its full health).
struct CampFight { bool fighting = false; vec2 camp{}; int enemy_dist = 0; float group_left = 1; };
CampFight camp_fight(const cambc::champion& e, const std::vector<cambc::camp>& camps, const MatchData& match) {
    CampFight cf;
    const auto p = e.position();
    const int r = threat_reach(design_for(static_cast<int>(e.champion_name())));
    const cambc::camp* nearest = nullptr;
    int best = 1 << 20;
    for (const auto& c : camps) {
        if (c.health() <= 0 || c.health() >= c.max_health()) continue;
        const int d = distance(p, c.position());
        if (d <= r && d < best) { best = d; nearest = &c; }
    }
    if (!nearest) return cf;
    cf.fighting = true;
    cf.camp = nearest->position();
    cf.enemy_dist = best;
    for (const auto& g : match.camp_groups) {
        if (!g.spawn_tiles.is_set(nearest->spawn_position())) continue;
        float left = 0;
        for (const auto& c : camps)
            if (c.health() > 0 && g.spawn_tiles.is_set(c.spawn_position())) left += c.health();
        if (g.design_health > 0) cf.group_left = left / g.design_health;
        break;
    }
    return cf;
}

// Targets this turn: the weakest enemy within reach we may attack now, and (kWaitForCampFights)
// whether some enemy within reach is one we are waiting on, with its camp fight.
struct Targets {
    const cambc::champion* attack = nullptr;
    const cambc::champion* chase = nullptr;   // kPursuePlan 2: a channelling gem carrier to walk to
    CampFight attack_camp;                 // the attack target's camp fight, if any
    std::optional<CampFight> waiting;
};
Targets pick_targets(int radius, const MatchData* match) {
    Targets t;
    const auto me = cambc::self().position();
    std::vector<cambc::camp> camps;
    if (constants::kWaitForCampFights && match) camps = cambc::vision::camps();
    for (const auto& e : roster_enemies()) {
        if (!in_view(e) || distance(e.position(), me) > radius) continue;
        if (constants::kWaitForCampFights && match) {
            const auto cf = camp_fight(e, camps, *match);
            const bool camp_low = cf.group_left <= constants::kWaitCampHealthFraction;
            const bool not_closest = distance(me, cf.camp) > cf.enemy_dist;
            if (cf.fighting && !camp_low && !not_closest) { t.waiting = cf; continue; }
            if (!t.attack || e.health() < t.attack->health()) { t.attack = &e; t.attack_camp = camp_low ? CampFight{} : cf; }
            continue;
        }
        if (!t.attack || e.health() < t.attack->health()) t.attack = &e;
    }
    if constexpr (constants::kPursuePlan >= 1) {
        // A recalling or tributing enemy in reach comes first: our hit cancels a recall, and a
        // tributing enemy carries gems (the weakest such enemy).
        const cambc::champion* channelling = nullptr;
        for (const auto& e : roster_enemies()) {
            if (!in_view(e) || distance(e.position(), me) > radius || !(e.is_recalling() || e.is_submitting_gems())) continue;
            if (!channelling || e.health() < channelling->health()) channelling = &e;
        }
        if (channelling) { t.attack = channelling; t.attack_camp = CampFight{}; }
    }
    if constexpr (constants::kPursuePlan >= 2) {
        // Nobody to hit: chase a channelling enemy carrying gems, if we are healthy enough.
        auto& self = cambc::self();
        if (!t.attack && !t.waiting && self.health() >= constants::kPursueMinHealth * self.max_health()) {
            int best = 1 << 20;
            for (const auto& e : roster_enemies()) {
                if (!in_view(e) || !(e.is_recalling() || e.is_submitting_gems())) continue;
                const int d = distance(e.position(), me);
                if (d > constants::kPursueRange || e.gem_count() < constants::kPursueMinGems) continue;
                if (d < best) { best = d; t.chase = &e; }
            }
        }
    }
    return t;
}
}
// Fight back whenever an enemy champion is within reach: the weakest one first. While a teammate's
// fight call is live (combat/sdk_team_fight.h, brain/team_fight_call.cpp), the called enemy comes
// first whenever it is within reach, and with nobody in reach we walk to it.
namespace {
// Fight value check (constants::kFightEv): is the fight around us worth taking?
bool fight_worth_it() {
    auto& me = cambc::self();
    const auto call = estimate_team_fight_here();
    if (call.enemies == 0) return true;
    const float p = call.estimate.call == TeamCall::Fight ? constants::kFightEvWinFight
                  : call.estimate.call == TeamCall::Avoid ? constants::kFightEvWinAvoid : constants::kFightEvWinUnsure;
    const auto here = me.position();
    const auto r = static_cast<std::uint32_t>(constants::kTeamFightRadius);
    float ours = static_cast<float>(me.gem_count()), theirs = 0;
    for (const auto& a : roster_allies())
        if (!(a == me) && in_view(a) && a.position().chebyshev_distance(here) <= r) ours += static_cast<float>(a.gem_count());
    for (const auto& e : roster_enemies())
        if (in_view(e) && e.position().chebyshev_distance(here) <= r) theirs += static_cast<float>(e.gem_count());
    const float kill = constants::kGoldWeight * static_cast<float>(cambc::constants::kill_gold);
    return p * (theirs + kill) > (1.0f - p) * (2.0f * ours + kill);
}
std::uint32_t last_attack_round = 0;   // last round we were in the attack state
}

int AttackState::score(const TurnContext& ctx) {
    // Outnumbered nearby: leave the fight (farm, tribute or recall take over).
    if constexpr (constants::kAvoidOutnumbered) {
        const auto here = cambc::self().position();
        const auto r = static_cast<std::uint32_t>(constants::kOutnumberRadius);
        int enemies = 0, allies = 0;
        for (const auto& e : roster_enemies())
            if (in_view(e) && e.position().chebyshev_distance(here) <= r) ++enemies;
        for (const auto& a : roster_allies())
            if (in_view(a) && a.position().chebyshev_distance(here) <= r) ++allies;   // includes us
        if (enemies > allies) return 0;
    }
    // Carrying many gems: stay out of fights and bank them (tribute) instead.
    if constexpr (constants::kCarrierAvoidGems > 0)
        if (cambc::self().gem_count() >= static_cast<std::uint32_t>(constants::kCarrierAvoidGems)) return 0;
    const auto t = pick_targets(reach(), ctx.match_data);
    if constexpr (constants::kFightEv > 0) {
        const auto round = cambc::round_number();
        const bool in_progress = last_attack_round + 1 >= round && last_attack_round != 0;
        if ((t.attack || t.chase) && (constants::kFightEv == 2 || !in_progress) && !fight_worth_it()) return 0;
    }
    if (t.attack || t.waiting || t.chase) return constants::kAttackScore;
    if (team_fight_called()) return constants::kTeamFightCallScore;
    return 0;
}

namespace {
// Walk toward the tiles within attack range of `spot`.
void walk_to(vec2 spot) { move_towards_closest(tiles_around(spot, cambc::self().attack_range())); }
}

StateResult AttackState::execute(TurnContext& ctx, BotMemory& memory) {
    auto& me = cambc::self();
    if constexpr (constants::kFightEv > 0) last_attack_round = cambc::round_number();
    const int r = reach();
    const auto targets = pick_targets(r, ctx.match_data);
    const auto* target = targets.attack;
    if (const auto order = team_fight_called()) {
        const auto& called = *order->target;
        const bool called_visible = in_view(called);
        if constexpr (constants::kTeamFightTestCaller && constants::kTeamFightTrace)
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
    if (!target && targets.chase) {
        // Pursuit (kPursuePlan 2): walk to the channelling gem carrier, hit it if we get there.
        cast_support();
        if (me.can_move()) walk_to(targets.chase->position());
        if (targets.chase->is_visible() && targets.chase->health() > 0) {
            cast_at(*targets.chase);
            attack_with_reset(*targets.chase);
        }
        return StateResult::Continue;
    }
    if (!target && targets.waiting) {
        // Waiting for the enemy to wear itself out on the camp: hold, and step back if the camp
        // would pick us (we are the closest to it).
        cast_support();
        if (me.can_move() && distance(me.position(), targets.waiting->camp) <= targets.waiting->enemy_dist) {
            const auto away = tiles_around(me.position(), 1) & ~tiles_around(targets.waiting->camp, targets.waiting->enemy_dist);
            move_towards_closest(away);
        }
        return StateResult::Continue;
    }
    if (!target) return StateResult::Done;
    cast_support();
    cast_at(*target);
    if (!attack_with_reset(*target) && me.can_move() && target->is_visible()) {
        // In ability range but not attack range: close in, then attack if we now can. Walk toward
        // the free tiles within attack range of the target: its own tile is occupied, so
        // move_towards(target position) never moves.
        auto goals = tiles_around(target->position(), me.attack_range());
        // Attacking an enemy busy with a camp: stay farther from the camp than the enemy, so the
        // camp keeps hitting it rather than us.
        if (targets.attack_camp.fighting) goals &= ~tiles_around(targets.attack_camp.camp, targets.attack_camp.enemy_dist);
        if constexpr ((constants::kDashPlan & 1) != 0) dash_towards(goals);
        if (!goals.is_set(me.position())) move_towards_closest(goals);
        if (target->is_visible() && target->health() > 0) attack_with_reset(*target);
    }
    return StateResult::Continue;
}
AttackState attack_state_instance;
