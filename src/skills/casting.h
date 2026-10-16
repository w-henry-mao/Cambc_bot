#pragma once
#include <cambc.h>
// Ability use shared by the states. Each tries at most one cast (the engine allows one per
// turn) and returns whether it cast. Engine refusals are caught, so a failed cast never costs
// the rest of the turn.

// A damage, area or stun ability on an enemy champion or a camp, if in range.
bool cast_at(const cambc::champion& target);
bool cast_at(const cambc::camp& target);
// A heal or shield on ourselves or the most hurt ally in range, if someone is below
// kSupportBelow of max health.
bool cast_support();
// Basic attack, then an attack reset and a second basic attack if available.
template <class Target> bool attack_with_reset(const Target& target);
