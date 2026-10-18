#include "draft.h"
#include <algorithm>
#include <string_view>
#include <vector>
#include "../constants/draft_constants.h"
#include "../constants/test_hooks.h"
#include "../core/group_routes.h"
namespace {
bool already_picked(const std::vector<cambc::champion_draft>& chosen, cambc::champion_draft pick) {
    return std::find(chosen.begin(), chosen.end(), pick) != chosen.end();
}
}
// Route lookahead builds: the camp group distance table for the champions, computed with the
// draft's spare fuel (~940k on its first turn, ~68k on later ones) and resubmitted every turn.
void build_routes() {
    static group_routes::Table table;
    static std::vector<cambc::vec2> centres;
    static cambc::bitboard walls;
    static bool started = false;
    if (!started) {
        started = true;
        const auto groups = cambc::map::camp_groups();
        if (groups.empty() || static_cast<int>(groups.size()) > group_routes::kMaxGroups) return;
        table.n = static_cast<int>(groups.size());
        table.dist.assign(static_cast<std::size_t>(table.n * table.n), group_routes::kFar);
        for (const auto& g : groups) centres.push_back(group_routes::group_centre(g));
        walls = group_routes::route_walls(groups);
    }
    if (table.n == 0) return;
    constexpr int kReserve = 40000;   // the pick still has to be made this turn
    while (table.rows_done < table.n - 1 && cambc::fuel_remaining() > kReserve)
        group_routes::compute_row(table, table.rows_done, walls, centres);
    cambc::draft_memory = group_routes::pack(table);
}

void play_draft() {
    const auto chosen = cambc::draft::current_draft(cambc::team::ally);
    if constexpr (ROUTE_LOOKAHEAD > 0) build_routes();
    else cambc::draft_memory = std::string_view("", 0);
    // Duel test builds may take one champion first (constants/test_hooks.h); -1 in normal builds.
    if constexpr (DUEL_FIRST_PICK >= 0) {
        const auto first = static_cast<cambc::champion_draft>(DUEL_FIRST_PICK);
        if (!already_picked(chosen, first)) { cambc::draft::pick(first); return; }
    }
    // Test builds may set the whole draft (constants/test_hooks.h DRAFT_PICKS); -1 in normal builds.
    constexpr int kTestPicks[] = {DRAFT_PICKS};
    if constexpr (kTestPicks[0] >= 0) {
        for (const int p : kTestPicks) {
            const auto pick = static_cast<cambc::champion_draft>(p);
            if (!already_picked(chosen, pick)) { cambc::draft::pick(pick); return; }
        }
    }
    for (const auto pick : constants::draft_priority) {
        if (!already_picked(chosen, pick)) {
            cambc::draft::pick(pick);
            return;
        }
    }
    // Priority list exhausted: take the lowest-numbered champion still free.
    for (int i = 0; i < 11; ++i) {
        const auto pick = static_cast<cambc::champion_draft>(i);
        if (!already_picked(chosen, pick)) {
            cambc::draft::pick(pick);
            return;
        }
    }
}
