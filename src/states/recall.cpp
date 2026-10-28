#include "recall.h"

int RecallState::score(const TurnContext& ctx) {
    return 0;
}

StateResult RecallState::execute(TurnContext& ctx, BotMemory& memory) {
    return StateResult::Done;
}

RecallState recall_state_instance;
