#pragma once
#include "../core/bot_memory.h"
struct MatchData;

// Returns true while combat owns this turn, including turns spent pursuing.
bool combat_turn(MatchData& match, BotMemory& memory);
