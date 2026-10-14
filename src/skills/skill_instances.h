#pragma once
#include "skill.h"
class MeleeAttack; class RangedAttack; class Dash; class AttackReset;
class SingleTargetDamage; class AoeDamage; class Stun; class Shield;
class Heal; class Ward; class Consumables;
extern MeleeAttack melee_attack_instance; extern RangedAttack ranged_attack_instance;
extern Dash dash_instance; extern AttackReset attack_reset_instance;
extern SingleTargetDamage single_target_damage_instance; extern AoeDamage aoe_damage_instance;
extern Stun stun_instance; extern Shield shield_instance; extern Heal heal_instance;
extern Ward ward_instance; extern Consumables consumables_instance;
