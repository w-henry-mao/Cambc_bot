#pragma once
// Test builds can override selected constants (tools/make_test_variant.py writes
// test_overrides.h, which is git-ignored). Normal builds see the defaults.
#if __has_include("test_overrides.h")
#include "test_overrides.h"
#endif
#ifndef COMBAT_TRACE
#define COMBAT_TRACE false
#endif
#ifndef DUEL_MODE
#define DUEL_MODE false
#endif
#ifndef DUEL_FIGHTER_TYPE
#define DUEL_FIGHTER_TYPE -1
#endif
#ifndef RECALL_SCORE
#define RECALL_SCORE 100
#endif
#ifndef FAST_SIM_FUEL_BUDGET
#define FAST_SIM_FUEL_BUDGET 15000
#endif
#ifndef REACH_BONUS
#define REACH_BONUS 0
#endif
#ifndef DUEL_FIRST_PICK
#define DUEL_FIRST_PICK -1
#endif
