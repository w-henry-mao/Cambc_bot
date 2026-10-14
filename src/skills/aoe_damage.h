#pragma once
#include "skill.h"
class AoeDamage : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern AoeDamage aoe_damage_instance;
