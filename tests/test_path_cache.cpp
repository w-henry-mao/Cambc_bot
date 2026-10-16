#include "test.h"
#include <vector>
#include "combat/design_table.h"
#include "combat/path_cache.h"
#include "combat/simulator.h"
// Path reuse: every point recorded on a fast prediction's path, fed back in as a fresh call,
// must give exactly the stored continuation (same outcome, health, and turns left).
static Fighter live(int type, float hp_frac, int cd0, int cd1) {
    const auto& d = design_for(type);
    Fighter f;
    f.design = &d; f.max_health = d.base_health; f.health = d.base_health * hp_frac;
    f.strength = d.base_strength; f.focus = d.base_focus; f.cooldown[0] = cd0; f.cooldown[1] = cd1;
    return f;
}
TEST(test_path_points_reproduce_fresh_results) {
    int checked = 0, mismatches = 0;
    for (int us_t : {10, 3, 4, 8, 5, 0})
        for (int en_t = 0; en_t < 12; ++en_t)
            for (float hp : {1.0f, 0.6f})
                for (int d : {1, 3, 5})
                    for (int phase = 0; phase < 3; ++phase) {
                        DuelSetup s;
                        s.us = live(us_t, hp, phase, 0); s.enemy = live(en_t, 1.0f, 0, phase * 2);
                        s.walk_distance = d;
                        s.enemy_acted_this_round = phase == 1;
                        s.same_turn_position = phase != 2;
                        std::vector<PathPoint> path;
                        FightOptions o; o.path_out = &path;
                        const auto r = simulate_duel_fast(s, o);
                        if (!r.resolved) continue;
                        for (const auto& p : path) {
                            const auto fresh = simulate_duel_fast(p.setup, {});
                            ++checked;
                            if (fresh.we_win != r.we_win || fresh.resolved != r.resolved
                                || std::fabs(fresh.our_health_worst_case - r.our_health_worst_case) > 1e-3
                                || std::fabs(fresh.enemy_health_at_end - r.enemy_health_at_end) > 1e-3
                                || fresh.turns != r.turns - p.turn) ++mismatches;
                        }
                    }
    CHECK(checked > 1000);
    CHECK(mismatches == 0);
}
TEST(test_reuse_only_on_an_exact_path_point) {
    DuelSetup s;
    s.us = live(10, 1.0f, 0, 0); s.enemy = live(0, 1.0f, 0, 0); s.walk_distance = 1;
    StoredPath stored;
    FightOptions o; o.path_out = &stored.points;
    stored.result = simulate_duel_fast(s, o);
    CHECK(stored.result.resolved); CHECK(stored.points.size() > 3);
    const auto& p = stored.points[3];
    const auto hit = reuse_from_path(stored, p.setup);
    CHECK(hit.has_value());
    if (hit) { CHECK(hit->turns == stored.result.turns - p.turn); CHECK(hit->we_win == stored.result.we_win); }
    auto off = p.setup; off.us.health -= 1;              // anything different: no reuse
    CHECK(!reuse_from_path(stored, off).has_value());
    off = p.setup; off.enemy.cooldown[0] += 1;
    CHECK(!reuse_from_path(stored, off).has_value());
    off = p.setup; off.enemy_acted_this_round = !off.enemy_acted_this_round;
    CHECK(!reuse_from_path(stored, off).has_value());
}
// The lookup stops early (healths only fall along a path); every point must still be found.
TEST(test_every_path_point_is_found) {
    int points = 0, found = 0;
    for (int en_t : {0, 1, 3, 9, 10})
        for (int d : {1, 4}) {
            DuelSetup s;
            s.us = live(10, 1.0f, 0, 0); s.enemy = live(en_t, 1.0f, 0, 0); s.walk_distance = d;
            StoredPath stored;
            FightOptions o; o.path_out = &stored.points;
            stored.result = simulate_duel_fast(s, o);
            if (!stored.result.resolved) continue;
            for (const auto& p : stored.points) {
                ++points;
                const auto hit = reuse_from_path(stored, p.setup);
                found += hit.has_value() && hit->turns == stored.result.turns - p.turn;
            }
        }
    CHECK(points > 50);
    CHECK(found == points);
}
TEST(test_path_recording_stops_at_the_limit) {
    DuelSetup s;
    s.us = live(10, 1.0f, 0, 0); s.enemy = live(0, 1.0f, 0, 0); s.walk_distance = 1;
    std::vector<PathPoint> path;
    FightOptions o; o.path_out = &path; o.path_max_points = 3;
    simulate_duel_fast(s, o);
    CHECK(path.size() == 3);
}
