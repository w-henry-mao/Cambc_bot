#include "initializer.h"
#include "stages.h"

void run_init_stages(MatchData& match_data, BotMemory& memory) {
    stage_draft_memory_read(match_data, memory);
}
