#pragma once
#include "test_hooks.h"
namespace constants {
constexpr int kMinFuelForPathing = 30000;
// Prefer a straight step when several next steps are equally short (nav/pathfinding.cpp).
constexpr bool kStraightTieBreak = STRAIGHT_TIE_BREAK;
}
