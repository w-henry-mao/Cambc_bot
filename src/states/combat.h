#pragma once
#include "../core/bot_memory.h"
#include <optional>
struct MatchData;

// Returns true while combat owns this turn, including turns spent pursuing.
bool combat_turn(MatchData& match, BotMemory& memory, float engage_distance = 5.0f,
                 cambc::vec2* target_position = nullptr, bool keep_mode_while_visible = false);
