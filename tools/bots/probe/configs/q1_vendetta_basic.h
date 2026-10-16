#pragma once
// Q1/Q4: melee closes and basic-attacks every turn; on contact turn 3 also tries reset + attack.
constexpr int kPick = 2, kFighterType = 2, kStopDistance = 6;
inline void probe_act(Probe& p) {
    if (p.dist() > 1) p.step_toward();
    p.attack();
    if (p.contact_turn == 3) { p.cast_self(0); p.attack(); }
}
