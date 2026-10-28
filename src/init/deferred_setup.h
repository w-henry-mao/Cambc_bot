#pragma once
#include <cambc.h>
#include "../core/match_data.h"

void continue_deferred_setup(MatchData& match);
const cambc::bitboard& beside_tiles(MatchData& match, CachedCampGroup& group);
