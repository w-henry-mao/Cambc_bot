#pragma once
#include <variant>
#include <string>
#include "decoded_draft_memory.h"
enum class State;
struct RecallMemory {};
struct AttackMemory {};
struct TributeMemory {};
struct FarmMemory {};
struct BotMemory {
    std::variant<std::monostate, RecallMemory, AttackMemory, TributeMemory, FarmMemory> current_state;
    int init_stage = 0;
    DecodedDraftMemory draft_memory;
    std::string champion_name;
};
