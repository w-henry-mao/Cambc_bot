#pragma once
#include <optional>
#include "state_module.h"
class FarmState : public StateModule {
public:
    int score(const TurnContext& ctx) override;
    StateResult execute(TurnContext& ctx, BotMemory& memory) override;
};
extern FarmState farm_state_instance;
struct MatchData;
// This round's farm target (computed once per round, shared with the recall score).
std::optional<int> farm_target(MatchData& match);
