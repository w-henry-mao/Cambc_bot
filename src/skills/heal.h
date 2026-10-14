#pragma once
#include "skill.h"
class Heal : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern Heal heal_instance;
