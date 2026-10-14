#include "ranged_attack.h"
bool RangedAttack::can_use(const TurnContext& ctx) { return false; }
void RangedAttack::use(TurnContext& ctx) {}
RangedAttack ranged_attack_instance;
