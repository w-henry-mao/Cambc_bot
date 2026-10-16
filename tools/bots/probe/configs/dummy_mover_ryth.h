#pragma once
// Dummy that tries to step toward the enemy every turn (so a stun shows as a failed move).
constexpr int kPick = 0, kFighterType = 0, kStopDistance = 6;
inline void probe_act(Probe& p) { if (p.dist() > 1) p.step_toward(); }
