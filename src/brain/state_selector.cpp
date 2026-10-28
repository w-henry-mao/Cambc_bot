#include "state_selector.h"

void StateSelector::add_state(StateModule* module, State state_enum) {
}

void StateSelector::select_and_execute(TurnContext& ctx, BotMemory& memory) {
}

StateModule* StateSelector::get_module_for_state(State state) {
    return nullptr;
}

State last_selected_state() {
    return State::None;
}
