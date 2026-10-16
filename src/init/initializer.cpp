#include "initializer.h"
#include "stages.h"
#include "../core/match_data.h"
#include "../constants/brain_constants.h"
static const StageFunc stages[] = {stage_map_snapshot, stage_roster, stage_self_setup, stage_draft_memory_read};
static constexpr int kNumStages = sizeof(stages) / sizeof(stages[0]);
// memory.init_stage counts finished stages, so a run cut short by the fuel reserve resumes at the
// next stage instead of repeating the last one (which duplicated every cached camp group).
void run_init_stages(MatchData& match_data, BotMemory& memory) {
    if (match_data.initialized) return;
    for (int stage_index = memory.init_stage + 1; stage_index <= kNumStages; ++stage_index) {
        if (!stages[stage_index - 1](match_data)) return;
        memory.init_stage = stage_index;
        if (cambc::fuel_remaining() < constants::kInitFuelReserve) return;
    }
    match_data.initialized = true;
}
