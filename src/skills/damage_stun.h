#pragma once
#include "skill.h"
class DamageStun : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern DamageStun damage_stun_instance;
