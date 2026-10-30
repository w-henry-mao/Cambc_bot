#pragma once
#include <cambc.h>
#include "../core/match_data.h"

void ensure_spawn_data(CachedCampGroup& group);
const cambc::bitboard& beside_tiles(MatchData& match, CachedCampGroup& group);
