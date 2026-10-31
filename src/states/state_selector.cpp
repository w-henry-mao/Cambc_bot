#include "state_selector.h"
#include "combat.h"
#include "farm.h"
#include "tribute.h"

void run_state_selector(const ChampionProfile& profile, MatchData& match, BotMemory& memory) {
    update_tribute_progress(memory);
    if (final_tribute_active()) {
        if (combat_turn(match, memory)) return;
        const bool force_recall = final_tribute_recall_phase();
        const bool recall_handled = recall_turn(match, memory, force_recall);
        if (memory.recall_channel_started) pause_final_tribute_for_recall(memory);
        if (recall_handled) return;
        if (final_tribute_turn(memory)) return;
    }
    if (recall_turn(match, memory)) return;
    if (combat_turn(match, memory)) return;
    if (tribute_turn(memory)) return;
    farm_turn(profile, match, memory);
}
