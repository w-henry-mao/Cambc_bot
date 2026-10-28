#include "attack.h"

int AttackState::score(const TurnContext& ctx) {
    return 0;
}

StateResult AttackState::execute(TurnContext& ctx, BotMemory& memory) {
    return StateResult::Done;
}

AttackState attack_state_instance;
