#pragma once
// Rough team-fight estimate (2v2, 3v3, or any mix up to kTeamFightMaxSide a side). Unlike the
// duel simulator this is not a worst-case search: it plays the fight round by round with
// simple rules and asks whether the answer survives a safety margin. Pure C++: no SDK calls.
//
// Rules (estimate only):
// - Each champion fights like states/attack.cpp: it attacks the opponent with the least health
//   within its reach (our_policy.h reach()); the enemy side attacks whichever opponent within
//   its reach dies fastest (least health + shield after damage reduction). With nobody in reach
//   a champion walks (dashing when ready) and every distance from it shrinks by the step, so
//   positions are only approximate. Melee walks and attacks in one turn; ranged does not.
// - Every round, every champion that is not stunned acts once, in our bot's order
//   (states/attack.cpp, skills/casting.cpp): one cast (heal/shield when below kSupportBelow, else
//   a ready damage/stun spell on its target), the basic attack, then an attack reset for a
//   second hit when no other cast was used. Our champions move first; damage from both sides
//   lands together at the end of the round. Stuns land on the target.
// - Area spells and Varg's splash hit only the target (no bonus for clumping). Camps, items
//   other than Durability, and terrain are ignored.
// - Cassi and Issac die together.
#include <cstdint>
#include "model.h"

inline constexpr int kTeamFightMaxSide = 6;   // 4 picks, Cassi & Issac count as two bodies

struct TeamMember {
    Fighter f;
};
struct TeamFightSetup {
    TeamMember us[kTeamFightMaxSide];
    int us_count = 0;
    TeamMember enemy[kTeamFightMaxSide];
    int enemy_count = 0;
    int distance[kTeamFightMaxSide][kTeamFightMaxSide] = {};   // [ours][theirs], Chebyshev tiles
};

struct TeamOutcome {
    bool we_win = false, they_win = false;   // neither: both wiped in the same round, or round cap
    int rounds = 0;
    float our_health_left = 0, enemy_health_left = 0;   // fraction of each side's total max health
    int our_alive = 0, enemy_alive = 0;      // still in the fight (not dead, not gone home)
    int our_dead = 0, enemy_dead = 0;
    int steps = 0;                           // quick version: events processed (a fuel proxy)
};

// Modelling switches for the quick version (team_fight_quick.cpp). Defaults: everyone fights to
// the end.
struct TeamFightOptions {
    // A champion stops fighting below this fraction of its max health and heads home (our recall
    // state does this, constants/state_constants.h kRecallHpFraction); 0 = never.
    float our_leave_below = 0, enemy_leave_below = 0;
    // Rounds a leaving champion can still be hit (walking away, or a recall channel) before it
    // is gone. It deals no damage meanwhile.
    float leave_delay = 0;
    // Champions farther than this from every opponent stay out of the fight (our attack state
    // only engages enemies within reach; others keep farming). 0 = everyone joins.
    int join_distance = 0;
    // Calibration of the averaged effects (1 = as modelled): spell damage (rate and burst), stun
    // time taken off the target, and heal/shield relief.
    float spell_weight = 1, stun_weight = 1, support_weight = 1;
};

enum class TeamCall : std::uint8_t { Fight, Avoid, Unsure };

struct TeamEstimate {
    TeamCall call = TeamCall::Unsure;
    // Fight: the run with the enemy's damage x margin. Avoid: the run with ours x margin.
    // Unsure: the latter.
    TeamOutcome outcome;
    int focus_target = -1;   // index into setup.enemy: the one with the least health; -1 if none
    int runs = 0;            // simulations run (1 or 2)
};

// One run with each side's damage (attacks, spells, retaliation) scaled; heals are not scaled.
TeamOutcome simulate_team_fight(const TeamFightSetup& setup, float our_damage_mult, float enemy_damage_mult, int round_cap);
// Fight: we still win when the enemy deals `margin` x its damage. Avoid: they still win when we
// deal `margin` x ours. Otherwise Unsure.
TeamEstimate estimate_team_fight(const TeamFightSetup& setup, float margin, int round_cap);
// Event-driven version (team_fight_quick.cpp): same inputs and answers, a fraction of the fuel,
// rougher (spell and stun timing averaged out). The team picks which one to call.
TeamOutcome simulate_team_fight_quick(const TeamFightSetup& setup, float our_damage_mult, float enemy_damage_mult, int round_cap,
                                      const TeamFightOptions& options = {});
TeamEstimate estimate_team_fight_quick(const TeamFightSetup& setup, float margin, int round_cap, const TeamFightOptions& options = {});
// The enemy with the least health (the one a fight call names); -1 if there is none.
int first_focus_target(const TeamFightSetup& setup);
