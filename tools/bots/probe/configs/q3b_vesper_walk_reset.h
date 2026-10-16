#pragma once
// Q3b: ranged at distance 4 walks to 3, then attack reset, then attack, in one turn.
constexpr int kPick = 1, kFighterType = 1, kStopDistance = 7;
inline bool done = false;
inline void probe_act(Probe& p) {
    if (done) return;
    if (p.dist() > 4) { p.step_toward(); return; }
    p.step_toward(); p.cast_self(0); p.attack(); done = true;
}
