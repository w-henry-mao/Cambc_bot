#pragma once
#include <variant>
enum class State;
struct RecallMemory {};
struct AttackMemory {};
struct TributeMemory {};
struct FarmMemory {};
struct BotMemory {
    std::variant<std::monostate, RecallMemory, AttackMemory, TributeMemory, FarmMemory> current_state;
    int init_stage = 0;
};
