#pragma once
#include "skill.h"
class Consumables : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern Consumables consumables_instance;
