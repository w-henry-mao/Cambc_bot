#include "our_policy.h"
#include <algorithm>
#include "../constants/test_hooks.h"
bool is_offensive(const AbilityDesign& a) {
    return a.kind == AbilityKind::Damage || a.kind == AbilityKind::AoeDamage || a.kind == AbilityKind::Cc;
}
bool is_support(const AbilityDesign& a) { return a.kind == AbilityKind::Heal || a.kind == AbilityKind::Shield; }
int offensive_slot(const PolicyView& v) {
    if (!v.can_cast) return -1;
    for (int i = 0; i < 2; ++i) {
        const auto& a = v.design->slots[i];
        if (is_offensive(a) && v.cooldown[i] == 0 && a.targets_enemies && static_cast<float>(v.distance) <= a.range) return i;
    }
    return -1;
}
int support_slot_self(const PolicyView& v) {
    if (!v.can_cast || v.health_fraction >= kSupportBelow) return -1;
    for (int i = 0; i < 2; ++i) {
        const auto& a = v.design->slots[i];
        if (is_support(a) && v.cooldown[i] == 0 && a.targets_self) return i;
    }
    return -1;
}
int reset_slot(const PolicyView& v) {
    if (!v.can_cast) return -1;
    for (int i = 0; i < 2; ++i)
        if (v.design->slots[i].kind == AbilityKind::AttackReset && v.cooldown[i] == 0) return i;
    return -1;
}
bool can_hit(const PolicyView& v) { return v.can_attack && v.distance <= v.design->attack_range; }
int reach(const ChampionDesign& d) {
    int r = d.attack_range + REACH_BONUS;   // test builds may widen engagement (aggression variants)
    for (const auto& a : d.slots)
        if (is_offensive(a)) r = std::max(r, static_cast<int>(a.range) + REACH_BONUS);
    return r;
}
