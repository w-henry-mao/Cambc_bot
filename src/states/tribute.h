#pragma once

#include "../core/bot_memory.h"
#include <optional>
#include <cstdint>

void update_tribute_progress(BotMemory& memory);
bool tribute_turn(BotMemory& memory);
void update_final_tribute(BotMemory& memory);
bool final_tribute_active();
bool final_tribute_recall_phase();
std::optional<std::uint32_t> prism_final_tribute_turns_to_completion(const BotMemory& memory);
void pause_final_tribute_for_recall(BotMemory& memory);
bool final_tribute_turn(BotMemory& memory, cambc::vec2* selected_target = nullptr);
