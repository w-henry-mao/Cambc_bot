#pragma once
#include "state_module.h"
class TributeState : public StateModule {
public:
    int score(const TurnContext& ctx) override;
    StateResult execute(TurnContext& ctx, BotMemory& memory) override;
};
extern TributeState tribute_state_instance;

// Altar-lock memory (constants::kAltarMemory): update from what we see this turn.
struct MatchData;
void note_altar_locks(MatchData& match);
