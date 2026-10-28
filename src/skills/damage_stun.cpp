#include "damage_stun.h"

bool DamageStun::can_use(const TurnContext& ctx) {
    return false;
}

void DamageStun::use(TurnContext& ctx) {
}

DamageStun damage_stun_instance;
