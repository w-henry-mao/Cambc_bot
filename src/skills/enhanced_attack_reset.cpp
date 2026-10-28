#include "enhanced_attack_reset.h"

bool EnhancedAttackReset::can_use(const TurnContext& ctx) {
    return false;
}

void EnhancedAttackReset::use(TurnContext& ctx) {
}

EnhancedAttackReset enhanced_attack_reset_instance;
