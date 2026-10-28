#include "aoe_damage.h"

bool AoeDamage::can_use(const TurnContext& ctx) {
    return false;
}

void AoeDamage::use(TurnContext& ctx) {
}

AoeDamage aoe_damage_instance;
