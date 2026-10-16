#pragma once
// Public fight API: live SDK state in, FightResult out. The model itself is in model.h and
// simulator.h. Call only with a visible enemy (live stats need vision).
#include <cambc.h>
#include "model.h"

// Our champion (self) against `enemy`. Fast: for normal turns. Full: for turns with spare fuel.
FightResult predict_duel_fast(const cambc::champion& enemy, const FightOptions& options = {});
FightResult predict_duel_full(const cambc::champion& enemy, const FightOptions& options = {});
// The model's view of a duel between two visible champions (`ours` plays our side).
DuelSetup duel_setup(const cambc::champion& ours, const cambc::champion& enemy, const FightOptions& options = {});
// Same, with any visible allied champion in our place.
FightResult predict_duel(const cambc::champion& ours, const cambc::champion& enemy, bool full, const FightOptions& options);
// Rough two-against-one: quick health check, else either ally winning the duel alone.
bool two_v_one_favourable(const cambc::champion& ally_a, const cambc::champion& ally_b, const cambc::champion& enemy);
// Fuel spent by the last uncached predict_duel call: reading SDK state, and simulating.
std::uint64_t last_setup_fuel();
std::uint64_t last_sim_fuel();
// Whether the last predict_duel call was answered from a stored path (no simulation).
bool last_call_hit_path_cache();
