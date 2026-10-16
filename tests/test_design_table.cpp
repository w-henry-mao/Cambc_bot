#include "test.h"
#include "combat/design_table.h"
// Values from docs/GameRules.md; the generated table comes from the engine (tools/data/designs.jsonl).
TEST(test_design_values) {
    const auto& ryth = design_for(0);
    CHECK_NEAR(ryth.base_health, 1200, 0.01); CHECK_NEAR(ryth.base_strength, 50, 0.01); CHECK_NEAR(ryth.base_focus, 100, 0.01);
    CHECK(!ryth.ranged); CHECK(ryth.attack_range == 1);
    CHECK(ryth.slots[0].kind == AbilityKind::AoeDamage); CHECK(ryth.slots[0].duration == 1);  // area stun
    const auto& stellar = design_for(5);
    CHECK(stellar.slots[0].kind == AbilityKind::Damage); CHECK_NEAR(stellar.slots[0].modifier, 2.0, 1e-4);
    CHECK_NEAR(stellar.slots[0].range, 4, 1e-4);
    const auto& varg = design_for(10);
    CHECK(varg.slots[1].kind == AbilityKind::Shield); CHECK_NEAR(varg.slots[1].modifier, 2.5, 1e-4);
    CHECK(varg.passive == Passive::SplashAttack);
    const auto& willow = design_for(8);
    CHECK(willow.slots[0].kind == AbilityKind::Heal); CHECK_NEAR(willow.slots[0].modifier, 3.0, 1e-4);
    const auto& cassi = design_for(3);
    CHECK(cassi.ranged); CHECK(cassi.attack_range == 3);
    CHECK(design_for(7).passive == Passive::StunImmunity);
    CHECK(design_for(9).passive == Passive::Retaliation);
    for (int t = 0; t < 12; ++t) {
        CHECK(design_for(t).type == t);
        CHECK(design_for(t).move_speed == 1);
        for (const auto& s : design_for(t).slots) CHECK(s.kind == AbilityKind::None || s.cooldown == 8);
    }
}
TEST(test_unknown_type_is_safe) {
    CHECK(design_for(12).type == -1);
    CHECK(design_for(-1).type == -1);
    CHECK(design_for(99).slots[0].kind == AbilityKind::None);
}
