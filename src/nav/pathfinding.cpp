#include "pathfinding.h"

bool move_towards_closest(cambc::bitboard goals) {
    auto& me = cambc::self();
    if (!me.can_move() || goals.is_empty()) return false;

    const auto walls = cambc::map::walls() | cambc::map::holes();
    cambc::distance_field field;
    cambc::bitboard visited;
    field.compute(walls, goals, visited);

    if (auto direction = field.next_move_from(me.position())) {
        return me.move(*direction);
    }
    return false;
}
