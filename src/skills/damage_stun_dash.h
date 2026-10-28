#pragma once
#include "skill.h"
class DamageStunDash : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern DamageStunDash damage_stun_dash_instance;
