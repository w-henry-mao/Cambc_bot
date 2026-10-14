#pragma once
#include "state_module.h"
class RecallState : public StateModule {
public:
    int score(const TurnContext& ctx) override;
    StateResult execute(TurnContext& ctx, BotMemory& memory) override;
};
extern RecallState recall_state_instance;
