#pragma once
#include "skill.h"
class Stun : public Skill {
public:
    bool can_use(const TurnContext& ctx) override;
    void use(TurnContext& ctx) override;
};
extern Stun stun_instance;
