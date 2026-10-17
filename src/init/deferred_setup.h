#pragma once
#include <cambc.h>
#include "../core/match_data.h"
// Setup that play can do without for a few turns: the walk home (safe recall scores tiles without
// it until it is ready) and the tiles beside every camp group's spawn tiles. It runs at the end of each turn, one step at a time, only while at least
// kDeferredSetupReserve fuel is left, and carries on at the end of the next turn until done.
void continue_deferred_setup(MatchData& match);
// Open tiles next to `group`'s spawn tiles, none of them a spawn tile of any group; computed now if
// the deferred setup has not reached the group yet.
const cambc::bitboard& beside_tiles(MatchData& match, CachedCampGroup& group);
