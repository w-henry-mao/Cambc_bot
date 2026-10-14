#pragma once
#include <cambc.h>
using cambc::vec2;
inline int distance(vec2 a, vec2 b) {
    return static_cast<int>(a.chebyshev_distance(b));
}
inline cambc::bitboard tiles_around(vec2 position, int radius) {
    cambc::bitboard tiles;
    tiles.clear();
    tiles.draw_chebyshev(position, radius);
    return tiles;
}
