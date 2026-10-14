#pragma once
#include "skill.h"
class SingleTargetDamage : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern SingleTargetDamage single_target_damage_instance;
