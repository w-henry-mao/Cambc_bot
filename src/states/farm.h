#pragma once
#include "../core/bot_memory.h"
struct ChampionProfile;
struct MatchData;
void farm_turn(const ChampionProfile& profile, MatchData& match, BotMemory& memory);
bool retreat_to_spawn(BotMemory& memory, MatchData& match, bool include_enemy_threats);
bool recall_turn(MatchData& match, BotMemory& memory, bool force_recall = false);
