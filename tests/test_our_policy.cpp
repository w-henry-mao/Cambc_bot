#include "test.h"
#include "combat/design_table.h"
#include "combat/our_policy.h"
// Encodes how states/attack.cpp + skills/casting.cpp fight today.
static PolicyView view_for(int type, int distance) {
    PolicyView v;
    v.design = &design_for(type);
    v.distance = distance;
    return v;
}
TEST(test_offensive_slot_in_range) {
    auto v = view_for(5, 4);                  // Stellar: slot 0 damage r4, slot 1 aoe r4
    CHECK(offensive_slot(v) == 0);
    v.cooldown[0] = 3;
    CHECK(offensive_slot(v) == 1);            // first ready offensive slot in slot order
    v.distance = 5;
    CHECK(offensive_slot(v) == -1);           // out of range
}
TEST(test_cc_counts_as_offensive) {
    auto v = view_for(3, 2);                  // Cassi: slot 0 damage, slot 1 stun (cc)
    v.cooldown[0] = 1;
    CHECK(offensive_slot(v) == 1);
}
TEST(test_no_offensive_when_cannot_cast) {
    auto v = view_for(5, 1);
    v.can_cast = false;
    CHECK(offensive_slot(v) == -1);
}
TEST(test_attack_in_range) {
    auto v = view_for(2, 1);                  // Vendetta melee
    CHECK(can_hit(v));
    v.distance = 2;
    CHECK(!can_hit(v));
    auto r = view_for(1, 3);                  // Vesper ranged 3
    CHECK(can_hit(r));
}
TEST(test_reset_slot_after_attack) {
    auto v = view_for(2, 1);                  // Vendetta slot 0 attack reset
    CHECK(reset_slot(v) == 0);
    v.cooldown[0] = 2;
    CHECK(reset_slot(v) == -1);
}
TEST(test_support_self_shield_below_threshold) {
    auto v = view_for(10, 1);                 // Varg slot 1 self-shield
    v.health_fraction = 0.81f;
    CHECK(support_slot_self(v) == -1);        // kSupportBelow = 0.8
    v.health_fraction = 0.79f;
    CHECK(support_slot_self(v) == 1);
    auto w = view_for(8, 1);                  // Willow heal cannot target self
    w.health_fraction = 0.1f;
    CHECK(support_slot_self(w) == -1);
}
TEST(test_reach_matches_attack_state) {
    CHECK(reach(design_for(10)) == 1);   // Varg: melee, no damage spell
    CHECK(reach(design_for(2)) == 1);    // Vendetta
    CHECK(reach(design_for(5)) == 4);    // Stellar: spells reach 4
    CHECK(reach(design_for(1)) == 3);    // Vesper Vale: ranged 3, no damage spell
    CHECK(reach(design_for(0)) == 4);    // Ryth: area stun range 4
}
