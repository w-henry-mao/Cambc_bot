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
// This round's farm target (computed once per round, shared with the recall score; `force`
// recomputes it).
std::optional<int> farm_target(MatchData& match, bool force = false);
// STATE_TRACE: what the farm state did this turn (H hit a camp, h camp in range but no attack,
// C capturing, S stepping off, W waiting beside a respawning group, A approaching a live group,
// E an earlier return: retreat, rally or regroup; '-' when the farm state did not run).
extern char g_farm_action;
// With action A/W/C: distance to the nearest live camp seen, can move, live camps seen (-1: unset).
extern int g_farm_detail[3];
