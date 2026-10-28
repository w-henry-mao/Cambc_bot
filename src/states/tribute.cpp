#include "tribute.h"

int TributeState::score(const TurnContext& ctx) {
    return 0;
}

StateResult TributeState::execute(TurnContext& ctx, BotMemory& memory) {
    return StateResult::Done;
}

TributeState tribute_state_instance;

void note_altar_locks(MatchData& match) {
}
