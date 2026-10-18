#include <cambc.h>
#include "../constants/test_hooks.h"
#include "../core/group_routes.h"
#include "../core/match_data.h"
#include "stages.h"
#include "../comms/draft_memory.h"
// Route lookahead builds: the camp group distance table the draft left (core/group_routes.h);
// without one, every row is computed in the deferred setup.
bool stage_draft_memory_read(MatchData& match_data) {
    if constexpr (ROUTE_LOOKAHEAD > 0) {
        auto& routes = match_data.routes;
        const int n = static_cast<int>(match_data.camp_groups.size());
        if (n > group_routes::kMaxGroups) return true;
        if (!group_routes::unpack(cambc::draft_memory, n, routes)) {
            routes.n = n;
            routes.rows_done = 0;
            routes.dist.assign(static_cast<std::size_t>(n * n), group_routes::kFar);
        }
    }
    return true;
}
