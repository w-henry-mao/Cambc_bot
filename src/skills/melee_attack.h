#pragma once
#include "skill.h"
class MeleeAttack : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern MeleeAttack melee_attack_instance;
