#include "test.h"
#include "combat/simulator.h"
#include "combat/our_policy.h"
// Hand-checked duels with tiny champions (no abilities unless a test adds them).
// Turn order when the enemy has not acted this round: us, enemy, enemy, us, us, enemy, ...
static ChampionDesign plain(bool ranged, int range) {
    ChampionDesign d;
    d.type = 99; d.ranged = ranged; d.attack_range = range;
    return d;
}
static Fighter fighter(const ChampionDesign& d, float hp, float str, float focus = 0) {
    Fighter f;
    f.design = &d; f.health = hp; f.max_health = hp; f.strength = str; f.focus = focus;
    return f;
}
static DuelSetup duel(Fighter us, Fighter enemy, int distance, bool enemy_acted = false) {
    DuelSetup s; s.us = us; s.enemy = enemy; s.walk_distance = distance; s.enemy_acted_this_round = enemy_acted;
    return s;
}
static const ChampionDesign kMelee = plain(false, 1), kRanged = plain(true, 3);

TEST(test_basic_trade) {
    // U hits 60, E hits 60, E hits 20, U hits 20, U kills.
    const auto r = simulate_duel(duel(fighter(kMelee, 100, 40), fighter(kMelee, 100, 40), 1), {});
    CHECK(r.resolved); CHECK(r.we_win);
    CHECK_NEAR(r.our_health_worst_case, 20, 1e-3); CHECK_NEAR(r.enemy_health_at_end, -20, 1e-3); CHECK(r.turns == 5);
}
TEST(test_outrange_free_shots) {
    // Ranged us (30) at distance 3 vs melee (40): U 70 | E walk | E walk+hit us 60 | U 40 | U 10 | E 20 | E dead.
    const auto r = simulate_duel(duel(fighter(kRanged, 100, 30), fighter(kMelee, 100, 40), 3), {});
    CHECK(r.resolved); CHECK(!r.we_win);
    CHECK_NEAR(r.our_health_worst_case, 0, 1e-3); CHECK_NEAR(r.enemy_health_at_end, 10, 1e-3); CHECK(r.turns == 7);
}
TEST(test_spell_burst_at_range_four) {
    // Enemy spell 1 x Focus 50, range 4, walk-excluding; enemy has no basic damage. We one-shot when adjacent.
    ChampionDesign caster = plain(false, 1);
    caster.slots[0] = AbilityDesign{AbilityKind::Damage, 4, 1, 0, 8, 0, true, false, true, false, false};
    // Our melee (reach 1) waits: U wait | E cast 50 | E walk (d3) | U wait | U wait | E walk (d2) |
    // E walk (d1) | U kill.
    const auto r = simulate_duel(duel(fighter(kMelee, 100, 1000), fighter(caster, 1000, 0, 50), 4), {});
    CHECK(r.we_win); CHECK_NEAR(r.our_health_worst_case, 50, 1e-3); CHECK(r.turns == 8);
}
static ChampionDesign stunner() {
    ChampionDesign d = plain(false, 1);
    d.slots[0] = AbilityDesign{AbilityKind::Cc, 4, 0, 0, 8, 1, false, false, true, false, false};
    return d;
}
TEST(test_stun_never_helps_us) {
    const auto with_stun = stunner();
    const auto a = simulate_duel(duel(fighter(kMelee, 300, 40), fighter(with_stun, 300, 40), 1), {});
    const auto b = simulate_duel(duel(fighter(kMelee, 300, 40), fighter(kMelee, 300, 40), 1), {});
    CHECK(a.our_health_worst_case <= b.our_health_worst_case);
}
TEST(test_prestunned_loses_first_turn) {
    // U stunned (skip) | E 60 | E 20 | U 60 | U 20 | E kills.
    auto us = fighter(kMelee, 100, 40); us.stunned_rounds = 1;
    const auto r = simulate_duel(duel(us, fighter(kMelee, 100, 40), 1), {});
    CHECK(!r.we_win); CHECK_NEAR(r.enemy_health_at_end, 20, 1e-3); CHECK(r.turns == 6);
}
TEST(test_marcella_ignores_stun) {
    ChampionDesign immune = plain(false, 1); immune.passive = Passive::StunImmunity;
    const auto st = stunner();
    const auto a = simulate_duel(duel(fighter(immune, 300, 40), fighter(st, 300, 40), 1), {});
    const auto b = simulate_duel(duel(fighter(immune, 300, 40), fighter(kMelee, 300, 40), 1), {});
    CHECK_NEAR(a.our_health_worst_case, b.our_health_worst_case, 1e-3); CHECK(a.we_win == b.we_win);
}
TEST(test_shield_absorbs_first) {
    // Shield 50: E hit 40 -> shield 10; E hit 40 -> shield 0, hp 70; we kill on turn 4.
    auto us = fighter(kMelee, 100, 40); us.shield = 50; us.shield_turns = 5;
    const auto r = simulate_duel(duel(us, fighter(kMelee, 100, 40), 1), {});
    CHECK(r.we_win); CHECK_NEAR(r.our_health_worst_case, 70, 1e-3);
}
TEST(test_durability_reduction) {
    auto us = fighter(kMelee, 100, 40); us.damage_taken_mult = 0.5f;
    const auto r = simulate_duel(duel(us, fighter(kMelee, 100, 40), 1), {});
    CHECK(r.we_win); CHECK_NEAR(r.our_health_worst_case, 60, 1e-3);
}
TEST(test_ren_retaliation_kills_us_too) {
    // Our killing blow triggers 0.2 x Focus 150 = 30 retaliation on our 30 hp: both die = loss.
    ChampionDesign ren = plain(false, 1); ren.passive = Passive::Retaliation;
    const auto r = simulate_duel(duel(fighter(kMelee, 30, 100), fighter(ren, 50, 0, 150), 1), {});
    CHECK(!r.we_win); CHECK(r.resolved); CHECK_NEAR(r.our_health_worst_case, 0, 1e-3);
}
TEST(test_unknown_cooldowns_not_better) {
    ChampionDesign caster = plain(false, 1);
    caster.slots[0] = AbilityDesign{AbilityKind::Damage, 4, 2, 0, 8, 0, true, false, true, false, false};
    auto ready = fighter(caster, 300, 30, 40); auto later = ready; later.cooldown[0] = 5;
    const auto a = simulate_duel(duel(fighter(kMelee, 300, 30), ready, 2), {});
    const auto b = simulate_duel(duel(fighter(kMelee, 300, 30), later, 2), {});
    CHECK(a.our_health_worst_case <= b.our_health_worst_case);
}
TEST(test_round_cap_unresolved) {
    const auto r = simulate_duel(duel(fighter(kMelee, 100, 0), fighter(kMelee, 100, 0), 1), FightOptions{false, 10});
    CHECK(!r.resolved); CHECK(!r.we_win);
}
TEST(test_camp_damage_only_when_enabled) {
    auto s = duel(fighter(kMelee, 100, 40), fighter(kMelee, 100, 40), 1);
    s.camp_damage_to_us_per_round = 30;
    const auto off = simulate_duel(s, FightOptions{false, 60});
    const auto on = simulate_duel(s, FightOptions{true, 60});
    CHECK(off.we_win); CHECK_NEAR(off.our_health_worst_case, 20, 1e-3);
    CHECK(on.our_health_worst_case < off.our_health_worst_case);
}
TEST(test_reset_gives_second_hit) {
    // Our reset (ready) doubles our first turn: U 40+40 | E 40 | E 40 | U kills (enemy 100 -> 20 -> -20).
    ChampionDesign resetter = plain(false, 1);
    resetter.slots[0] = AbilityDesign{AbilityKind::AttackReset, 0, 0, 0, 8, 0, false, false, false, true, false};
    const auto r = simulate_duel(duel(fighter(resetter, 100, 40), fighter(kMelee, 100, 40), 1), {});
    CHECK(r.we_win); CHECK_NEAR(r.our_health_worst_case, 20, 1e-3); CHECK(r.turns == 4);
}
TEST(test_self_shield_fires_below_threshold) {
    // Varg-like: self-shield 1 x Focus 100 (ready), used once below 80% health.
    // Enemy hits 30 per turn on our 200 hp: 170, 140 (<160) -> shield on our next turn absorbs 100.
    ChampionDesign varg = plain(false, 1);
    varg.slots[1] = AbilityDesign{AbilityKind::Shield, 4, 1, 0, 8, 5, false, false, false, true, false};
    const auto shielded = simulate_duel(duel(fighter(varg, 200, 10, 100), fighter(kMelee, 1000, 30), 1), {});
    const auto plain_r = simulate_duel(duel(fighter(kMelee, 200, 10, 100), fighter(kMelee, 1000, 30), 1), {});
    CHECK(shielded.turns > plain_r.turns);          // the shield buys extra turns
    CHECK(!shielded.we_win); CHECK(!plain_r.we_win);
}
// Fake fuel counter for budget tests: every reading costs 100.
static std::uint64_t g_fake_fuel = 0;
static std::uint64_t fake_fuel() { g_fake_fuel -= 100; return g_fake_fuel; }
TEST(test_fuel_budget_stops_unresolved) {
    // A long fight (lots of health, small hits) with a tiny budget must stop, unresolved.
    g_fake_fuel = 1000000;
    FightOptions o; o.fuel_left = fake_fuel; o.fuel_budget = 300;
    const auto r = simulate_duel(duel(fighter(kMelee, 5000, 1), fighter(kMelee, 5000, 1), 1), o);
    CHECK(!r.resolved); CHECK(!r.we_win);
}
TEST(test_no_budget_means_unlimited) {
    g_fake_fuel = 1000000;
    FightOptions o; o.fuel_left = fake_fuel;   // budget 0 = unlimited
    const auto r = simulate_duel(duel(fighter(kMelee, 100, 40), fighter(kMelee, 100, 40), 1), o);
    CHECK(r.resolved); CHECK(r.we_win);
}
TEST(test_we_only_close_in_within_reach) {
    // Our melee (reach 1) never walks toward a ranged enemy shooting from 3, like the real bot
    // (it keeps farming until the enemy is adjacent). Enemy 30 per shot on our 100.
    const auto r = simulate_duel(duel(fighter(kMelee, 100, 1000), fighter(kRanged, 100, 30), 3), {});
    CHECK(!r.we_win); CHECK(r.resolved);
}
// The enemy's greedy plan for one turn (what the test aggressor bot plays).
TEST(test_enemy_plan_choice) {
    ChampionDesign agg = plain(false, 1);
    agg.slots[0] = AbilityDesign{AbilityKind::Damage, 4, 2, 0, 8, 0, true, false, true, false, false};
    agg.slots[1] = AbilityDesign{AbilityKind::AttackReset, 0, 0, 0, 8, 0, false, false, false, true, false};
    auto setup = duel(fighter(kMelee, 500, 10), fighter(agg, 500, 40, 50), 1);
    CHECK(choose_enemy_plan(setup).kind == PlanKind::Offensive);   // 100 spell + 40 > 80 from reset
    setup.enemy.cooldown[0] = 3;
    CHECK(choose_enemy_plan(setup).kind == PlanKind::Reset);       // two hits
    setup.enemy.cooldown[1] = 3;
    CHECK(choose_enemy_plan(setup).kind == PlanKind::Plain);
    ChampionDesign dasher = plain(false, 1);
    dasher.slots[0] = AbilityDesign{AbilityKind::Dash, 2, 0, 0, 8, 0, false, false, false, false, false};
    CHECK(choose_enemy_plan(duel(fighter(kMelee, 500, 10), fighter(dasher, 500, 40), 3)).kind == PlanKind::Dash);
}
// docs/combat-rules.md #3c: a ranged champion can walk, then reset, then attack in one turn.
static float g_hp_after_turn1 = -1;
static void record_turn1(int t, bool, const Fighter& us, const Fighter&, int) { if (t == 1) g_hp_after_turn1 = us.health; }
TEST(test_ranged_walk_reset_attack) {
    ChampionDesign ranged_reset = plain(true, 3);
    ranged_reset.slots[1] = AbilityDesign{AbilityKind::AttackReset, 0, 0, 0, 8, 0, false, false, false, true, false};
    FightOptions o; o.on_turn = record_turn1;
    g_hp_after_turn1 = -1;
    simulate_duel(duel(fighter(kMelee, 1000, 10), fighter(ranged_reset, 1000, 30), 4), o);
    CHECK_NEAR(g_hp_after_turn1, 970, 1e-3);   // turn 1 (enemy): walk to 3, reset, hit for 30
}
// A shield on 0 turns left still absorbs until its owner's next turn starts (GameRules: zero
// timers resolve at turn start; seen in the Varg duel trace).
static float g_hp_after_turn2 = -1;
static void record_turn2(int t, bool, const Fighter& us, const Fighter&, int) { if (t == 2) g_hp_after_turn2 = us.health; }
TEST(test_shield_expires_at_owner_turn_start) {
    auto us = fighter(kMelee, 1000, 0); us.shield = 100; us.shield_turns = 1;
    FightOptions o; o.on_turn = record_turn2; o.round_cap = 5;
    g_hp_after_turn2 = -1;
    simulate_duel(duel(us, fighter(kMelee, 1000, 40), 1), o);   // t0 us (1 -> 0), t1/t2 enemy hits 40 + 40
    CHECK_NEAR(g_hp_after_turn2, 1000, 1e-3);                   // both absorbed by the shield
}
// Our bot only runs its combat code (attack state) with the enemy within reach, so a ready
// self-shield is not cast while a ranged enemy shoots from outside our reach.
static bool g_shield_cast_out_of_reach = false;
static void watch_shield(int, bool, const Fighter& us, const Fighter&, int d) { if (us.shield > 0 && d > 1) g_shield_cast_out_of_reach = true; }
TEST(test_no_support_cast_out_of_reach) {
    ChampionDesign varg = plain(false, 1);
    varg.slots[1] = AbilityDesign{AbilityKind::Shield, 4, 2.5f, 0, 8, 5, false, false, false, true, false};
    FightOptions o; o.on_turn = watch_shield; o.round_cap = 15;
    g_shield_cast_out_of_reach = false;
    // Ranged enemy (range 3) never needs to come closer; we (reach 1) never engage.
    simulate_duel(duel(fighter(varg, 300, 50, 100), fighter(kRanged, 1000, 45), 3), o);
    CHECK(!g_shield_cast_out_of_reach);
}
// states/attack.cpp closes in toward tiles within attack range of its target
// (kAttackStateClosesIn), so with the enemy in reach but not in attack range we walk.
static bool g_we_moved = false;
static void watch_distance(int, bool ours, const Fighter&, const Fighter&, int d) { if (ours && d != 3) g_we_moved = true; }
TEST(test_attack_state_closes_in_within_reach) {
    ChampionDesign caster = plain(false, 1);   // melee with a range-4 spell: in reach at 3, not in attack range
    caster.slots[0] = AbilityDesign{AbilityKind::Damage, 4, 1, 0, 8, 0, true, false, true, false, false};
    auto us = fighter(caster, 1000, 50, 10); us.cooldown[0] = 8;
    FightOptions o; o.on_turn = watch_distance; o.round_cap = 10;
    g_we_moved = false;
    simulate_duel(duel(us, fighter(kRanged, 1000, 45), 3), o);   // ranged enemy stays at 3
    CHECK(g_we_moved == kAttackStateClosesIn);
}
#include "combat/budget.h"
// Review finding 1: the budget never exceeds the fuel actually left (minus a reserve).
TEST(test_budget_respects_fuel_left) {
    CHECK(prediction_budget(false, 200000, 15000, 25000) == 15000);   // plenty left: the cap
    CHECK(prediction_budget(false, 30000, 15000, 25000) == 5000);     // little left: what remains
    CHECK(prediction_budget(false, 20000, 15000, 25000) == 0);        // below the reserve: none
    CHECK(prediction_budget(true, 100000, 15000, 25000) == 75000);    // full: all but the reserve
    CHECK(prediction_budget(true, 900, 15000, 25000) == 0);
}
// Review finding 2: if the budget cut any branch short, the result must not claim to be resolved
// (an unexplored branch could be worse for us). Every budget either gives the unlimited answer
// or an unresolved one.
static int g_reads = 0, g_cut_at = 0;
static std::uint64_t jumping_fuel() { return ++g_reads < g_cut_at ? 1000000 : 0; }   // runs out at read K
TEST(test_search_cut_off_is_unresolved) {
    // Whenever the budget ran out at any point, some branch went unexplored, so the answer is not
    // a proven worst case and must come back unresolved.
    const auto st = stunner();
    const auto setup = duel(fighter(kMelee, 600, 40), fighter(st, 600, 40), 1);
    int cut_but_resolved = 0, cuts = 0;
    for (int k = 2; k < 400; ++k) {
        g_reads = 0; g_cut_at = k;
        FightOptions o; o.fuel_left = jumping_fuel; o.fuel_budget = 500000;
        const auto r = simulate_duel(setup, o);
        if (g_reads >= g_cut_at) { ++cuts; if (r.resolved) ++cut_but_resolved; }
    }
    CHECK(cuts > 0);
    CHECK(cut_but_resolved == 0);
}
// Review finding 3: a cached answer is only reused for identical inputs (not just healths).
TEST(test_same_inputs_compares_everything) {
    const auto a = duel(fighter(kMelee, 100, 40), fighter(kMelee, 100, 40), 2);
    auto b = a; CHECK(same_inputs(a, b));
    b = a; b.walk_distance = 3; CHECK(!same_inputs(a, b));
    b = a; b.enemy.cooldown[1] = 4; CHECK(!same_inputs(a, b));
    b = a; b.us.shield = 50; CHECK(!same_inputs(a, b));
    b = a; b.enemy.stunned_rounds = 1; CHECK(!same_inputs(a, b));
    b = a; b.enemy_acted_this_round = true; CHECK(!same_inputs(a, b));
    b = a; b.our_turn_now = false; CHECK(!same_inputs(a, b));
}
// Review finding 4: predicting for an ally that is not acting right now gives it no free first
// turn: the enemy moves first, which is never better for that ally.
TEST(test_ally_not_acting_now_moves_after_enemy) {
    auto now = duel(fighter(kMelee, 100, 40), fighter(kMelee, 100, 40), 1);
    auto later = now; later.our_turn_now = false;
    const auto a = simulate_duel(now, {});
    const auto b = simulate_duel(later, {});
    CHECK(a.we_win);            // basic trade: acting first wins
    CHECK(!b.we_win);           // the enemy's first hit decides it
    CHECK(b.our_health_worst_case <= a.our_health_worst_case);
}
