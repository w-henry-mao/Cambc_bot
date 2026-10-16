#pragma once
// Q3: ranged. dist 5 -> dash (2) then attack in same turn; later: walk then attack;
// later: attack, reset, attack; later: walk, then reset (no attack yet), attack.
constexpr int kPick = 1, kFighterType = 1, kStopDistance = 7;
inline int phase = 0;
inline void probe_act(Probe& p) {
    if (phase == 0) { if (p.dist() > 5) { p.step_toward(); return; } p.dash_toward(1); p.attack(); phase = 1; return; }
    if (phase == 1) { p.step_away(); p.step_away(); phase = 2; return; }            // second move should fail
    if (phase == 2) { if (p.dist() <= 3) { p.step_away(); return; } p.step_toward(); p.attack(); phase = 3; return; }
    if (phase >= 3 && phase < 12) { ++phase; if (p.dist() <= 3) p.attack(); else p.step_toward(); return; }
    if (phase == 12) { p.attack(); p.cast_self(0); p.attack(); phase = 13; return; }
    if (phase >= 13 && phase < 22) { ++phase; if (p.dist() <= 3) p.attack(); return; }
    if (phase == 22) { p.step_away(); p.cast_self(0); p.attack(); phase = 23; return; }
}
