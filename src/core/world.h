#pragma once
#include <cambc.h>
#include "match_data.h"
// Per-turn bookkeeping, called once at the start of each turn we act in.
void update_world(MatchData& match);
// Walking distance from `tile` to the nearest altar's tribute range (65535: unreachable).
int altar_walk(const MatchData& match, cambc::vec2 tile);
