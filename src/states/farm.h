#pragma once
#include "state_module.h"
class FarmState : public StateModule {
public:
    int score(const TurnContext& ctx) override;
    StateResult execute(TurnContext& ctx, BotMemory& memory) override;
};
extern FarmState farm_state_instance;
