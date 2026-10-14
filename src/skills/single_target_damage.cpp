#include "single_target_damage.h"
bool SingleTargetDamage::can_use(const TurnContext& ctx) { return false; }
void SingleTargetDamage::use(TurnContext& ctx) {}
SingleTargetDamage single_target_damage_instance;
