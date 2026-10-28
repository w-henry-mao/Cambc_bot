#pragma once
#include "skill.h"
class EnhancedAttackReset : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern EnhancedAttackReset enhanced_attack_reset_instance;
