#include "state_selector.h"
#include "combat.h"
#include "farm.h"

void run_state_selector(const ChampionProfile& profile, MatchData& match, BotMemory& memory) {
    if (recall_turn(match, memory)) return;
    if (combat_turn(match, memory)) return;
    farm_turn(profile, match, memory);
}
