#pragma once
#include <cambc.h>
#include "../core/match_data.h"
#include "../core/bot_memory.h"
using StageFunc = bool (*)(MatchData&);
void run_init_stages(MatchData& match_data, BotMemory& memory);
