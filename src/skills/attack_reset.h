#pragma once
#include "skill.h"
class AttackReset : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern AttackReset attack_reset_instance;
