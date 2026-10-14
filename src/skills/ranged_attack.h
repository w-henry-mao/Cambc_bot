#pragma once
#include "skill.h"
class RangedAttack : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern RangedAttack ranged_attack_instance;
