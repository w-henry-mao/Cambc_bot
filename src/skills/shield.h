#pragma once
#include "skill.h"
class Shield : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern Shield shield_instance;
