#include "deferred_setup.h"
#include <vector>
#include "../constants/brain_constants.h"

namespace {
// Tiles a waiting champion may stand on: not a spawn tile of any group, not a static blocker.
void ensure_beside_mask(MatchData& match) {
    if (match.beside_mask_ready) return;
    cambc::bitboard spawn;
    spawn.clear();
    for (const auto& group : match.camp_groups) spawn |= group.spawn_tiles;
    match.beside_mask = ~spawn & ~match.static_blockers;
    match.beside_mask_ready = true;
}

// Walking distance to our spawn region (~25k fuel on a 100x100 map, the largest step).
void build_home_distance(MatchData& match) {
    cambc::bitboard visited;
    match.home_distance.compute(match.static_blockers, cambc::map::spawn_region(cambc::team::ally), visited);
    match.home_ready = true;
}
}  // namespace

const cambc::bitboard& beside_tiles(MatchData& match, CachedCampGroup& group) {
    if (!group.beside_ready) {
        // Drawn onto one board: building a fresh board per tile cost ~10k fuel for a 4-tile group.
        ensure_beside_mask(match);
        group.beside_tiles.clear();
        for (const auto tile : group.spawn_positions) group.beside_tiles.draw_chebyshev(tile, 1.0f);
        group.beside_tiles &= match.beside_mask;
        group.beside_ready = true;
    }
    return group.beside_tiles;
}

void continue_deferred_setup(MatchData& match) {
    if (!match.initialized) return;
    while (!match.deferred_done && cambc::fuel_remaining() >= constants::kDeferredSetupReserve) {
        const int step = match.deferred_step++;
        const int groups = static_cast<int>(match.camp_groups.size());
        if (step == 0) build_home_distance(match);
        else if (step - 1 < groups) beside_tiles(match, match.camp_groups[static_cast<std::size_t>(step - 1)]);
        else match.deferred_done = true;
    }
}
