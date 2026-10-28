#include "attack_reset.h"

bool AttackReset::can_use(const TurnContext& ctx) {
    return false;
}

void AttackReset::use(TurnContext& ctx) {
}

AttackReset attack_reset_instance;
