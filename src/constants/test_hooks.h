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
#ifndef VERIFY_PATH_CACHE
#define VERIFY_PATH_CACHE false
#endif
#ifndef PATH_CACHE_ENABLED
#define PATH_CACHE_ENABLED true
#endif
#ifndef TEAM_FIGHT_TRACE
#define TEAM_FIGHT_TRACE false
#endif
#ifndef TEAM_FIGHT_TEST_CALLER
#define TEAM_FIGHT_TEST_CALLER false
#endif
// Test builds: draft exactly these champion_draft values in order (comma list), e.g. 1,2,5,6,8.
#ifndef DRAFT_PICKS
#define DRAFT_PICKS -1
#endif
// Test builds: the test caller re-estimates every N rounds (staggered by roster index).
#ifndef TEAM_FIGHT_TEST_CALL_EVERY
#define TEAM_FIGHT_TEST_CALL_EVERY 1
#endif
// Overrides for the fight-call lifetime and the shared-teammate limits (test builds compare).
#ifndef TEAM_FIGHT_CALL_ROUNDS
#define TEAM_FIGHT_CALL_ROUNDS 2
#endif
#ifndef TEAM_FIGHT_SHARED_RADIUS
#define TEAM_FIGHT_SHARED_RADIUS 8
#endif
#ifndef ALLY_POSITION_MAX_AGE
#define ALLY_POSITION_MAX_AGE 3
#endif
