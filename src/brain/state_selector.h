#pragma once
#include <cambc.h>
#include <vector>
#include "state.h"
#include "../states/state_module.h"
#include "../core/bot_memory.h"
#include "../constants/brain_constants.h"
struct TurnContext;
class StateSelector {
public:
    void add_state(StateModule* module, State state_enum);
    void select_and_execute(TurnContext& ctx, BotMemory& memory);
private:
    StateModule* get_module_for_state(State state);
    struct StateEntry { StateModule* module; State state_enum; };
    std::vector<StateEntry> states_;
};
// The state chosen on the most recent selection this turn (State::None if none).
State last_selected_state();
