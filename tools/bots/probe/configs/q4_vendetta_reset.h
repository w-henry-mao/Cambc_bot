#pragma once
// Q4/Q7: once adjacent for 2 turns: attack, reset, attack in one turn; then keep attacking (cooldown log).
constexpr int kPick = 2, kFighterType = 2, kStopDistance = 6;
inline int adjacent_turns = 0;
inline void probe_act(Probe& p) {
    if (p.dist() > 1) { p.step_toward(); p.attack(); return; }
    ++adjacent_turns;
    p.attack();
    if (adjacent_turns == 2) { p.cast_self(0); p.attack(); }
}
