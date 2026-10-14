#include "melee_attack.h"
bool MeleeAttack::can_use(const TurnContext& ctx) { return false; }
void MeleeAttack::use(TurnContext& ctx) {}
MeleeAttack melee_attack_instance;
