#pragma once
#include <cambc.h>
#include "../core/match_data.h"

void ensure_spawn_data(CachedCampGroup& group);
const cambc::bitboard& all_camp_spawn_tiles(MatchData& match);
const cambc::bitboard& beside_tiles(MatchData& match, CachedCampGroup& group);
