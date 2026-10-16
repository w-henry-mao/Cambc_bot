#include "casting.h"
#include "../core/roster.h"
namespace {
constexpr float kSupportBelow = 0.8f;

bool offensive(const cambc::ability& ab) { return ab.is_damage() || ab.is_aoe_damage() || ab.is_cc(); }

template <class Target> bool in_attack_range(const Target& target) {
    return target.is_visible() && target.health() > 0 && cambc::self().attack_tiles().is_set(target.position());
}

template <class Target> bool cast_offensive(const Target& target, bool camp) {
    auto& me = cambc::self();
    if (!me.can_cast() || !target.is_visible() || target.health() <= 0) return false;
    for (const auto& ab : me.abilities()) {
        try {
            if (!offensive(ab) || !ab.is_ready()) continue;
            if (camp ? !ab.can_target_camps() : !ab.can_target_enemies()) continue;
            if (!ab.valid_tiles().is_set(target.position())) continue;
            if (ab.use(target)) return true;
        } catch (const cambc::error&) {}
    }
    return false;
}
}

bool cast_at(const cambc::champion& target) { return cast_offensive(target, false); }
bool cast_at(const cambc::camp& target) { return cast_offensive(target, true); }

bool cast_support() {
    auto& me = cambc::self();
    if (!me.can_cast()) return false;
    for (const auto& ab : me.abilities()) {
        try {
            if (!(ab.is_heal() || ab.is_shield()) || !ab.is_ready()) continue;
            const cambc::champion* best = nullptr;
            float best_missing = 0;
            const auto tiles = ab.valid_tiles();
            for (const auto& ally : roster_allies()) {
                if (!in_view(ally)) continue;
                const bool is_me = ally == me;
                if (is_me ? !ab.can_target_self() : !(ab.can_target_allies() && tiles.is_set(ally.position()))) continue;
                if (ally.health() >= kSupportBelow * ally.max_health()) continue;
                const float missing = ally.max_health() - ally.health();
                if (missing > best_missing) { best = &ally; best_missing = missing; }
            }
            if (best && ab.use(*best)) return true;
        } catch (const cambc::error&) {}
    }
    return false;
}

template <class Target> bool attack_with_reset(const Target& target) {
    auto& me = cambc::self();
    if (!me.can_attack() || !in_attack_range(target)) return false;
    me.attack(target);
    if (!me.can_cast() || !in_attack_range(target)) return true;
    for (const auto& ab : me.abilities()) {
        try {
            if (ab.is_attack_reset() && ab.is_ready() && ab.use()) {
                if (me.can_attack() && in_attack_range(target)) me.attack(target);
                break;
            }
        } catch (const cambc::error&) {}
    }
    return true;
}
template bool attack_with_reset<cambc::champion>(const cambc::champion&);
template bool attack_with_reset<cambc::camp>(const cambc::camp&);
