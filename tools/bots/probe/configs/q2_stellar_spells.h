#pragma once
// Q2: spell damage vs damage_amount(); AoE (radius 1) on a lone target.
constexpr int kPick = 4, kFighterType = 5, kStopDistance = 6;
inline void probe_act(Probe& p) {
    if (p.dist() > 4) { p.step_toward(); return; }
    if (p.me.abilities()[0].is_ready()) { p.predict(0); p.cast_on_enemy(0); return; }
    if (p.me.abilities()[1].is_ready()) { p.predict(1); p.cast_ground(1, p.enemy->position()); return; }
    if (p.dist() <= 3) p.attack(); else p.step_toward();
}
