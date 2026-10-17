#include "pathfinding.h"
#include "blocked_tiles.h"
#include "../constants/nav_constants.h"
bool move_towards_closest(cambc::bitboard goals) {
    auto& me = cambc::self();
    if (!me.can_move() || goals.is_set(me.position())) return false;
    if (cambc::fuel_remaining() < constants::kMinFuelForPathing) return false;
    const auto blocked = blocked_tiles();
    goals &= ~blocked | cambc::vision::camp_tiles();
    if (goals.is_empty()) return false;
    if (goals.count() == 1 && !blocked.is_set(goals.positions()[0]))
        return me.move_towards(goals.positions()[0]);
    const auto path_walls = blocked & ~goals;
    cambc::distance_field field;
    cambc::bitboard visited;
    field.compute_to(path_walls, goals, tiles_around(me.position(), 0), visited);
    // Steps on a shortest path often tie (diagonals cost the same). Taking the first in tile order
    // drifts to low y, along the map edge; kStraightTieBreak prefers a straight step among ties.
    const auto here = me.position();
    auto step = here;
    auto best = field.get(here);
    bool step_diagonal = true;
    for (auto tile : blocked.reachable_tiles(here, me.base_stats().base_move_speed).positions()) {
        const auto d = field.get(tile);
        const bool diagonal = tile.x != here.x && tile.y != here.y;
        if (d < best || (constants::kStraightTieBreak && d == best && step != here && step_diagonal && !diagonal)) {
            best = d; step = tile; step_diagonal = diagonal;
        }
    }
    return step != here && me.move_absolute(step);
}
