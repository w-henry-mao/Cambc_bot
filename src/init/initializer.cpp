#include "initializer.h"
#include "stages.h"

void run_init_stages(MatchData& match_data) {
    if (match_data.initialized) return;
    if (!match_data.map_snapshot_ready) {
        stage_map_snapshot(match_data);
        match_data.map_snapshot_ready = true;
        if (cambc::fuel_remaining() < 1000) return;
    }
    match_data.initialized = true;
}
