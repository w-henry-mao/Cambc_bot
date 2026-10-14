#include <cambc.h>
#include "../core/match_data.h"
#include "stages.h"
#include "../champions/registry.h"
bool stage_self_setup(MatchData& match_data) {
    if (cambc::is_draft()) return true;
    return true;
}
