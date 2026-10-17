#pragma once
// Dashing toward a goal (Vesper, Vendetta, Issac and Ren have a dash). A dash is an ability cast
// and does not use the move allowance, so it combines with a normal step and an attack.
#include <cambc.h>

// Dash to the legal landing tile closest to `goals` (Chebyshev to the nearest goal tile), but only
// if that beats where one ordinary step could get us. True if we dashed.
bool dash_towards(const cambc::bitboard& goals);
