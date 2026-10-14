#pragma once
#include "skill.h"
class Dash : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern Dash dash_instance;
