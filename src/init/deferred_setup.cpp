#include "deferred_setup.h"

void ensure_spawn_data(CachedCampGroup& group) {
    if (group.spawn_ready || !group.view) return;
    group.view->spawn_tiles_into(group.spawn_tiles);
    group.spawn_positions.clear();
    for (const auto& camp : *group.view) {
        // These are immutable map definitions; live position()/state() require vision.
        group.spawn_positions.push_back(camp.spawn_position());
    }
    if (!group.spawn_positions.empty()) {
        group.centre = group.spawn_positions[group.spawn_positions.size() / 2];
    }
    group.spawn_ready = true;
}

const cambc::bitboard& beside_tiles(MatchData& match, CachedCampGroup& group) {
    if (!match.beside_mask_ready) {
        cambc::bitboard all_spawn_tiles;
        for (auto& cached : match.camp_groups) {
            ensure_spawn_data(cached);
            all_spawn_tiles |= cached.spawn_tiles;
        }
        match.beside_mask.fill();
        match.beside_mask &= ~match.static_blockers;
        match.beside_mask &= ~all_spawn_tiles;
        match.beside_mask_ready = true;
    }

    ensure_spawn_data(group);
    if (!group.beside_ready) {
        group.beside_tiles.clear();
        for (const auto position : group.spawn_positions) {
            group.beside_tiles.draw_chebyshev(position, 1);
        }
        group.beside_tiles &= match.beside_mask;
        group.beside_positions = group.beside_tiles.positions();
        group.beside_ready = true;
    }
    return group.beside_tiles;
}
