#include "farm.h"

int FarmState::score(const TurnContext& ctx) {
    return 0;
}

StateResult FarmState::execute(TurnContext& ctx, BotMemory& memory) {
    return StateResult::Done;
}

FarmState farm_state_instance;

std::optional<int> farm_target(MatchData& match, bool force) {
    return std::nullopt;
}

char g_farm_action = '-';
int g_farm_detail[3] = {-1, -1, -1};
