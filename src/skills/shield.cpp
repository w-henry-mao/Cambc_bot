#include "shield.h"

bool Shield::can_use(const TurnContext& ctx) {
    return false;
}

void Shield::use(TurnContext& ctx) {
}

Shield shield_instance;
