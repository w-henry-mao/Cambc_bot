#pragma once
#include "skill.h"
class Ward : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern Ward ward_instance;
