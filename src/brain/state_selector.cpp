#include "state_selector.h"
#include "../core/context.h"
#include "../core/bot_memory.h"
#include "../states/state_module.h"
#include "../debug/log.h"
#include <algorithm>

namespace { State last_selected = State::None; std::uint32_t last_selected_round = 0; }
State last_selected_state() { return last_selected_round == cambc::round_number() ? last_selected : State::None; }

void StateSelector::add_state(StateModule* module, State state_enum) {
    states_.push_back({module, state_enum});
}
void StateSelector::select_and_execute(TurnContext& ctx, BotMemory& memory) {
    std::vector<std::pair<State, int>> scores;
    for (const auto& entry : states_) {
        int score = entry.module->score(ctx);
        if (score > 0) scores.push_back({entry.state_enum, score});
    }
    last_selected_round = cambc::round_number();
    if (scores.empty()) { last_selected = State::None; return; }
    std::sort(scores.begin(), scores.end(),
        [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second > b.second;
            return static_cast<int>(a.first) < static_cast<int>(b.first);
        });
    State current_state = State::None;
    int current_score = 0;
    if (std::holds_alternative<RecallMemory>(memory.current_state)) {
        current_state = State::Recall; current_score = constants::kRecallScore;
    } else if (std::holds_alternative<AttackMemory>(memory.current_state)) {
        current_state = State::Attack; current_score = constants::kAttackScore;
    } else if (std::holds_alternative<TributeMemory>(memory.current_state)) {
        current_state = State::Tribute; current_score = constants::kTributeScore;
    } else if (std::holds_alternative<FarmMemory>(memory.current_state)) {
        current_state = State::Farm; current_score = constants::kFarmScore;
    }
    State chosen_state = State::None;
    StateModule* chosen_module = nullptr;
    if (current_state == State::None || current_score <= 0) {
        chosen_state = scores[0].first;
        chosen_module = get_module_for_state(chosen_state);
    } else {
        for (const auto& [state, score] : scores) {
            if (state == current_state) continue;
            if (score > current_score + constants::kDefaultSwitchMargin) {
                chosen_state = state;
                chosen_module = get_module_for_state(state);
                break;
            }
        }
        if (chosen_state == State::None) {
            chosen_state = current_state;
            chosen_module = get_module_for_state(current_state);
        }
    }
    last_selected = chosen_state;
    if (chosen_module) {
        try {
            chosen_module->execute(ctx, memory);
        } catch (const cambc::error&) {
            debug::log("State execution failed");
        }
    }
}
StateModule* StateSelector::get_module_for_state(State state) {
    for (const auto& entry : states_) {
        if (entry.state_enum == state) return entry.module;
    }
    return nullptr;
}
