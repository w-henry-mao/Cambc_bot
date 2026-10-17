#include "test.h"
#include "combat/team_fight.h"
#include "combat/design_table.h"
#include "comms/ally_position_bits.h"
// Hand-checked team fights with tiny champions (no abilities unless a test adds them).
static ChampionDesign plain(bool ranged, int range, int type = 99) {
    ChampionDesign d;
    d.type = type; d.ranged = ranged; d.attack_range = range;
    return d;
}
static TeamMember member(const ChampionDesign& d, float hp, float str, float focus = 0) {
    TeamMember m;
    m.f.design = &d; m.f.health = hp; m.f.max_health = hp; m.f.strength = str; m.f.focus = focus;
    return m;
}
// Every pair starts `distance` apart unless a test overrides setup.distance.
static TeamFightSetup fight(std::initializer_list<TeamMember> us, std::initializer_list<TeamMember> enemy, int distance = 1) {
    TeamFightSetup s;
    for (const auto& m : us) s.us[s.us_count++] = m;
    for (const auto& m : enemy) s.enemy[s.enemy_count++] = m;
    for (int i = 0; i < s.us_count; ++i)
        for (int j = 0; j < s.enemy_count; ++j) s.distance[i][j] = distance;
    return s;
}
static const ChampionDesign kMelee = plain(false, 1), kRanged = plain(true, 3);

TEST(test_team_two_beat_one) {
    // Round 0: we deal 80 (enemy 20), it deals 40 to us[0]. Round 1: it dies; its last hit lands too.
    const auto s = fight({member(kMelee, 100, 40), member(kMelee, 100, 40)}, {member(kMelee, 100, 40)});
    const auto r = simulate_team_fight(s, 1, 1, 40);
    CHECK(r.we_win); CHECK(!r.they_win); CHECK(r.rounds == 2);
    CHECK(r.our_alive == 2); CHECK_NEAR(r.our_health_left, 0.6, 1e-4);
    CHECK(estimate_team_fight(s, 1.3f, 40).call == TeamCall::Fight);
}
TEST(test_team_one_vs_two_avoid) {
    const auto s = fight({member(kMelee, 100, 40)}, {member(kMelee, 100, 40), member(kMelee, 100, 40)});
    const auto e = estimate_team_fight(s, 1.3f, 40);
    CHECK(e.call == TeamCall::Avoid); CHECK(e.runs == 2); CHECK(e.outcome.they_win);
}
TEST(test_team_mirror_is_unsure) {
    // Equal sides trade evenly: neither survives the margin either way.
    const auto s = fight({member(kMelee, 300, 40), member(kMelee, 300, 40)}, {member(kMelee, 300, 40), member(kMelee, 300, 40)});
    const auto even = simulate_team_fight(s, 1, 1, 40);
    CHECK(!even.we_win); CHECK(!even.they_win);
    CHECK(estimate_team_fight(s, 1.3f, 40).call == TeamCall::Unsure);
    // A large margin is still Unsure; margin 1 too (a draw is not a win).
    CHECK(estimate_team_fight(s, 1.0f, 40).call == TeamCall::Unsure);
}
TEST(test_team_margin_decides) {
    // 2v2 where we hit 1.5x as hard. Even at margin 1.3 the enemy (52) loses: we kill one in
    // 3 rounds (120 a round on 300), its pair (104 a round) kills our first in round 3 as well, then our
    // 60 beats its 52 one on one. At margin 1.6 (64 a hit) it wins the first trade outright.
    const auto s = fight({member(kMelee, 300, 60), member(kMelee, 300, 60)}, {member(kMelee, 300, 40), member(kMelee, 300, 40)});
    CHECK(estimate_team_fight(s, 1.3f, 40).call == TeamCall::Fight);
    CHECK(estimate_team_fight(s, 1.6f, 40).call == TeamCall::Unsure);
}
TEST(test_team_we_focus_least_health) {
    const auto s = fight({member(kMelee, 1000, 50)}, {member(kMelee, 500, 0), member(kMelee, 100, 0)});
    CHECK(first_focus_target(s) == 1);
    // Kill the 100 first (2 rounds), then the 500 (10 rounds).
    const auto r = simulate_team_fight(s, 1, 1, 40);
    CHECK(r.we_win); CHECK(r.rounds == 12);
}
TEST(test_team_cassi_issac_die_together) {
    const auto& cassi = design_for(3);
    const auto& issac = design_for(4);
    auto c = member(cassi, 50, 0); auto i = member(issac, 5000, 0);
    const auto s = fight({member(kMelee, 1000, 60)}, {c, i});
    const auto r = simulate_team_fight(s, 1, 1, 40);
    CHECK(r.we_win); CHECK(r.rounds == 1); CHECK(r.enemy_alive == 0);
}
TEST(test_team_walk_in_delay) {
    // Ranged us (reach 3, 30 a hit) and melee them (40) start 5 apart, both 100 health.
    // Round 0: we walk (5 -> 4), it walks (-> 3). Round 1: we shoot (70); it walks (-> 2).
    // Round 2: we shoot (40); it walks (-> 1) and hits (60). Round 3: we shoot (10); it hits (20).
    // Round 4: we kill it; its last hit lands too (-20): both die, a draw.
    const auto s = fight({member(kRanged, 100, 30)}, {member(kMelee, 100, 40)}, 5);
    const auto r = simulate_team_fight(s, 1, 1, 40);
    CHECK(!r.we_win); CHECK(!r.they_win); CHECK(r.rounds == 5);
    // One tile closer and the melee arrives a round earlier: it wins.
    CHECK(simulate_team_fight(fight({member(kRanged, 100, 30)}, {member(kMelee, 100, 40)}, 4), 1, 1, 40).they_win);
}
TEST(test_team_targets_only_within_reach) {
    // In a match, a lone Stellar against Cassi (3 tiles) and Issac (8 tiles) was called a fight:
    // the model let Stellar shoot the weaker Issac from 8 tiles and the linked death wiped both.
    // Stellar may only hit what is within its reach, so it shoots Cassi.
    const auto& stellar = design_for(5);
    const auto& cassi = design_for(3);
    const auto& issac = design_for(4);
    auto s = fight({member(stellar, 510, 35, 100)}, {member(cassi, 700, 30, 70), member(issac, 480, 40, 100)});
    s.distance[0][0] = 3;
    s.distance[0][1] = 8;
    const auto r = simulate_team_fight(s, 1, 1, 40);
    CHECK(!r.we_win);
    CHECK(estimate_team_fight(s, 1.3f, 40).call != TeamCall::Fight);
}
TEST(test_team_stun_skips_rounds) {
    ChampionDesign stunner = plain(false, 1);
    stunner.slots[0] = AbilityDesign{AbilityKind::Cc, 4, 0, 0, 8, 1, false, false, true, false, false};
    // Without the stun the mirror is a draw. The stun lands with round 0's hits, so the enemy
    // still hits in round 0, misses round 1, and hits once more as it dies in round 2.
    const auto s = fight({member(stunner, 100, 40)}, {member(kMelee, 100, 40)});
    const auto r = simulate_team_fight(s, 1, 1, 40);
    CHECK(r.we_win); CHECK(r.rounds == 3); CHECK_NEAR(r.our_health_left, 0.2, 1e-4);
    CHECK(!simulate_team_fight(fight({member(kMelee, 100, 40)}, {member(kMelee, 100, 40)}), 1, 1, 40).we_win);
}
TEST(test_team_heal_and_shield) {
    ChampionDesign healer = plain(true, 3);
    healer.slots[0] = AbilityDesign{AbilityKind::Heal, 4, 1, 0, 8, 0, false, false, false, false, true};
    // The healer (0 damage) tops up its melee partner once it drops below 80%.
    const auto with = fight({member(kMelee, 100, 40), member(healer, 1000, 0, 50)}, {member(kMelee, 120, 40)});
    const auto without = fight({member(kMelee, 100, 40), member(kRanged, 1000, 0, 50)}, {member(kMelee, 120, 40)});
    const auto a = simulate_team_fight(with, 1, 1, 40), b = simulate_team_fight(without, 1, 1, 40);
    CHECK(a.we_win); CHECK(a.our_alive == 2);
    CHECK(a.our_health_left > b.our_health_left);
    ChampionDesign shielder = plain(false, 1);
    shielder.slots[0] = AbilityDesign{AbilityKind::Shield, 4, 2, 0, 8, 5, false, false, false, true, false};
    const auto sh = simulate_team_fight(fight({member(shielder, 100, 40, 50)}, {member(kMelee, 100, 40)}), 1, 1, 40);
    CHECK(sh.we_win);   // the 100-point shield buys the deciding round
}
TEST(test_team_retaliation) {
    // Hitting Ren (Focus 100) costs 20 per hit: a 40-health attacker dies on its second hit.
    const auto& ren = design_for(9);
    auto r = member(ren, 1000, 0, 100);
    r.f.cooldown[0] = r.f.cooldown[1] = 99;   // no spells: retaliation only
    const auto out = simulate_team_fight(fight({member(kMelee, 40, 10)}, {r}), 1, 1, 40);
    CHECK(out.they_win); CHECK(out.rounds == 2);
}
TEST(test_team_real_designs_run) {
    // Every design against every other in a 2v2 finishes without crashing; a 3v1 of the same
    // design is always Fight at full health.
    for (int a = 0; a < kChampionTypeCount; ++a)
        for (int b = 0; b < kChampionTypeCount; ++b) {
            const auto& da = design_for(a); const auto& db = design_for(b);
            auto m = [](const ChampionDesign& d) {
                TeamMember t;
                t.f.design = &d; t.f.health = t.f.max_health = d.base_health; t.f.strength = d.base_strength;
                t.f.focus = d.base_focus;
                return t;
            };
            const auto s = fight({m(da), m(da)}, {m(db), m(db)}, 3);
            const auto e = estimate_team_fight(s, 1.3f, 40);
            CHECK(e.runs >= 1);
            if (a == b) {
                const auto big = fight({m(da), m(da), m(da)}, {m(db)}, 3);
                CHECK(estimate_team_fight(big, 1.3f, 40).call == TeamCall::Fight);
            }
        }
}

// ---- Event-driven version --------------------------------------------------------------
TEST(test_quick_clear_cases) {
    const auto two_v_one = fight({member(kMelee, 100, 40), member(kMelee, 100, 40)}, {member(kMelee, 100, 40)});
    CHECK(estimate_team_fight_quick(two_v_one, 1.3f, 40).call == TeamCall::Fight);
    const auto one_v_two = fight({member(kMelee, 100, 40)}, {member(kMelee, 100, 40), member(kMelee, 100, 40)});
    CHECK(estimate_team_fight_quick(one_v_two, 1.3f, 40).call == TeamCall::Avoid);
    const auto mirror = fight({member(kMelee, 300, 40), member(kMelee, 300, 40)}, {member(kMelee, 300, 40), member(kMelee, 300, 40)});
    CHECK(estimate_team_fight_quick(mirror, 1.3f, 40).call == TeamCall::Unsure);
    // Trade time: 100 health at 40 a round dies in 2.5 rounds.
    const auto r = simulate_team_fight_quick(two_v_one, 1, 1, 40);
    CHECK(r.we_win); CHECK(r.our_alive == 2);
}
TEST(test_quick_cassi_issac_and_reach) {
    const auto& cassi = design_for(3);
    const auto& issac = design_for(4);
    const auto r = simulate_team_fight_quick(fight({member(kMelee, 1000, 60)}, {member(cassi, 50, 0), member(issac, 5000, 0)}), 1, 1, 40);
    CHECK(r.we_win); CHECK(r.enemy_alive == 0);
    // A 20-tile gap only delays the fight.
    const auto far = simulate_team_fight_quick(fight({member(kMelee, 100, 40), member(kMelee, 100, 40)}, {member(kMelee, 100, 40)}, 20), 1, 1, 40);
    CHECK(far.we_win); CHECK(far.rounds >= 10);
}
TEST(test_quick_agrees_with_round_version) {
    // Not an exact match (the quick version averages), but on random fights from the real
    // designs it should rarely call the opposite (Fight vs Avoid).
    unsigned seed = 777;
    auto rnd = [&](int n) { seed = seed * 1103515245u + 12345u; return static_cast<int>((seed >> 16) % static_cast<unsigned>(n)); };
    int same = 0, opposite = 0, total = 0;
    for (int it = 0; it < 3000; ++it) {
        TeamFightSetup s;
        s.us_count = 1 + rnd(4);
        s.enemy_count = 1 + rnd(4);
        auto fill = [&](TeamMember& m) {
            const auto& d = design_for(rnd(kChampionTypeCount));
            const float level = static_cast<float>(rnd(10));
            m.f.design = &d;
            m.f.max_health = d.base_health + level * d.health_growth;
            m.f.health = std::max(1.0f, m.f.max_health * static_cast<float>(20 + rnd(81)) / 100.0f);
            m.f.strength = d.base_strength + level * d.strength_growth;
            m.f.focus = d.base_focus + level * d.focus_growth;
            m.f.cooldown[0] = rnd(9); m.f.cooldown[1] = rnd(9);
        };
        for (int i = 0; i < s.us_count; ++i) fill(s.us[i]);
        for (int i = 0; i < s.enemy_count; ++i) fill(s.enemy[i]);
        for (int i = 0; i < s.us_count; ++i)
            for (int j = 0; j < s.enemy_count; ++j) s.distance[i][j] = 1 + rnd(7);
        const auto a = estimate_team_fight(s, 1.3f, 40).call, b = estimate_team_fight_quick(s, 1.3f, 40).call;
        ++total;
        same += a == b;
        opposite += (a == TeamCall::Fight && b == TeamCall::Avoid) || (a == TeamCall::Avoid && b == TeamCall::Fight);
    }
    std::printf("  quick vs round: same call %d/%d, opposite %d\n", same, total, opposite);
    CHECK(opposite * 50 < total);   // under 2%
}
TEST(test_quick_options) {
    // Support weight scales heal relief: a healer pair that holds with full heals loses more
    // health with half and none.
    ChampionDesign healer = plain(true, 3);
    healer.slots[0] = AbilityDesign{AbilityKind::Heal, 4, 1, 0, 8, 0, false, false, false, true, true};
    const auto s = fight({member(kMelee, 400, 40), member(healer, 400, 10, 400)}, {member(kMelee, 600, 60)});
    TeamFightOptions full, half, none;
    half.support_weight = 0.5f;
    none.support_weight = 0;
    const auto a = simulate_team_fight_quick(s, 1, 1, 40, full), b = simulate_team_fight_quick(s, 1, 1, 40, half),
               c = simulate_team_fight_quick(s, 1, 1, 40, none);
    CHECK(a.our_health_left >= b.our_health_left); CHECK(b.our_health_left >= c.our_health_left);
    CHECK(a.our_health_left > c.our_health_left);
    // Leaving: with our champions leaving below 35%, nobody of ours dies in a lost 1v2.
    TeamFightOptions leave;
    leave.our_leave_below = 0.35f;
    const auto lost = fight({member(kMelee, 300, 40)}, {member(kMelee, 300, 40), member(kMelee, 300, 40)});
    const auto l = simulate_team_fight_quick(lost, 1, 1, 40, leave);
    CHECK(l.our_dead == 0); CHECK(!l.we_win);
    CHECK(simulate_team_fight_quick(lost, 1, 1, 40).our_dead == 1);
    // Join distance: an enemy 10 tiles away stays out, so a 1v2 becomes a 1v1 mirror.
    TeamFightOptions join;
    join.join_distance = 6;
    auto far = fight({member(kMelee, 300, 40)}, {member(kMelee, 300, 40), member(kMelee, 300, 40)});
    far.distance[0][1] = 10;
    CHECK(simulate_team_fight_quick(far, 1, 1, 40, join).enemy_alive <= 1);
}
TEST(test_ally_position_bits) {
    // Every tile of a 100x100 map, every roster slot, both health flags: back to the 2-tile cell.
    for (int x = 0; x < 100; ++x)
        for (int y = 0; y < 100; y += 7)
            for (int idx = 0; idx < 6; ++idx)
                for (int h = 0; h < 2; ++h) {
                    int ux, uy, ui;
                    bool uh;
                    unpack_ally_position(pack_ally_position(x, y, idx, h == 1), ux, uy, ui, uh);
                    CHECK(ux == x - x % 2); CHECK(uy == y - y % 2); CHECK(ui == idx); CHECK(uh == (h == 1));
                }
    // Off-map values clamp rather than spill into other fields.
    int ux, uy, ui;
    bool uh;
    unpack_ally_position(pack_ally_position(500, -3, 2, false), ux, uy, ui, uh);
    CHECK(ux == 126); CHECK(uy == 0); CHECK(ui == 2); CHECK(!uh);
}
