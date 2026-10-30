#pragma once
#include "../core/bot_memory.h"
struct ChampionProfile;
struct MatchData;
void farm_turn(const ChampionProfile& profile, MatchData& match, BotMemory& memory);
