#include "initializer.h"
#include "stages.h"
#include "../core/match_data.h"
#include "../constants/brain_constants.h"
#include "../debug/fuel_meter.h"
static const StageFunc stages[] = {stage_map_snapshot, stage_roster, stage_self_setup, stage_draft_memory_read};
static constexpr int kNumStages = sizeof(stages) / sizeof(stages[0]);
void run_init_stages(MatchData& match_data, BotMemory& memory) {
    if (match_data.initialized) return;
    int stage_index = memory.init_stage > 0 ? memory.init_stage : 1;
    for (; stage_index <= kNumStages; ++stage_index) {
        debug::FuelMeter meter("init_stage");
        bool done = stages[stage_index - 1](match_data);
        if (done) memory.init_stage = stage_index;
        else return;
        if (cambc::fuel_remaining() < constants::kInitFuelReserve) return;
    }
    match_data.initialized = true;
}
