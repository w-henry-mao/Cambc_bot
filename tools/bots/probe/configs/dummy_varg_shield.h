#pragma once
// Q6: Varg shields itself on contact turn 2, then stands still.
constexpr int kPick = 9, kFighterType = 10, kStopDistance = 6;
inline void probe_act(Probe& p) { if (p.contact_turn == 6) p.cast_self(1); }
