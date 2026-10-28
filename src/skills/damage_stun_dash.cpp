#include "damage_stun_dash.h"

bool DamageStunDash::can_use(const TurnContext& ctx) {
    return false;
}

void DamageStunDash::use(TurnContext& ctx) {
}

DamageStunDash damage_stun_dash_instance;
