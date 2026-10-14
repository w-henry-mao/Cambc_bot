#pragma once
#include "state_module.h"
class AttackState : public StateModule {
public:
    int score(const TurnContext& ctx) override;
    StateResult execute(TurnContext& ctx, BotMemory& memory) override;
};
extern AttackState attack_state_instance;
