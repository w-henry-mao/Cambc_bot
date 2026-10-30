#pragma once
#include "../core/bot_memory.h"
struct ChampionProfile;
struct MatchData;

void run_state_selector(const ChampionProfile& profile, MatchData& match, BotMemory& memory);
