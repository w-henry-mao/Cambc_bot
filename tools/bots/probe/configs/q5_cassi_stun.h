#pragma once
// Q5: Cassi stuns (slot 1) once in range, then basic-attacks; the dummy logs its own turns.
constexpr int kPick = 3, kFighterType = 3, kStopDistance = 6;
inline bool stunned_once = false;
inline void probe_act(Probe& p) {
    if (p.dist() > 4) { p.step_toward(); return; }
    if (!stunned_once && p.me.abilities()[1].is_ready()) { p.cast_on_enemy(1); stunned_once = true; return; }
    if (p.dist() <= 3) p.attack();
}
