#include "test.h"
#include "nav/safe_spot_score.h"
#include "combat/design_table.h"
#include "combat/our_policy.h"

static const SpotWeights kW{1.0f, 0.5f, 0.25f, 0.25f, 3.0f, 12};

TEST(test_spot_fewer_steps_and_closer_home_win) {
    SpotFeatures near_tile, far_tile;
    near_tile.steps = 2; near_tile.home = 20;
    far_tile.steps = 5; far_tile.home = 20;
    CHECK(spot_score(near_tile, kW) > spot_score(far_tile, kW));
    SpotFeatures homeward = far_tile;
    homeward.home = 10;   // 3 more steps but 10 tiles closer to home (0.5 each) wins
    CHECK(spot_score(homeward, kW) > spot_score(near_tile, kW));
}
TEST(test_spot_bush_bonus) {
    SpotFeatures plain, bush;
    plain.steps = 2; plain.home = 20;
    bush.steps = 4; bush.home = 20; bush.bush = true;   // 2 extra steps (-2) vs bonus +3
    CHECK(spot_score(bush, kW) > spot_score(plain, kW));
    bush.steps = 6;                                     // 4 extra steps: plain wins
    CHECK(spot_score(bush, kW) < spot_score(plain, kW));
}
TEST(test_spot_friends_and_enemies) {
    SpotFeatures a, b;
    a.friend_dist = 2; b.friend_dist = 10;
    CHECK(spot_score(a, kW) > spot_score(b, kW));
    SpotFeatures c, d;
    c.enemy_dist = 11; d.enemy_dist = 4;
    CHECK(spot_score(c, kW) > spot_score(d, kW));
    // Beyond the cap, more distance from enemies adds nothing.
    SpotFeatures e, f;
    e.enemy_dist = 12; f.enemy_dist = 30;
    CHECK_NEAR(spot_score(e, kW), spot_score(f, kW), 1e-6);
    // No teammate / no other enemy visible: those terms are left out.
    SpotFeatures g;
    CHECK_NEAR(spot_score(g, kW), 0, 1e-6);
}
TEST(test_threat_reach) {
    CHECK(threat_reach(design_for(2)) == 1);    // vendetta: melee, no damage spell
    CHECK(threat_reach(design_for(1)) == 3);    // vesper: ranged basic attack
    CHECK(threat_reach(design_for(0)) == 4);    // ryth: area stun range 4
    CHECK(threat_reach(design_for(8)) == 3);    // willow: heal does not threaten
    for (int t = 0; t < kChampionTypeCount; ++t) CHECK(threat_reach(design_for(t)) >= design_for(t).attack_range);
}
