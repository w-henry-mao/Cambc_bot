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
// Safe recall on (true) or the old walk-home recall (false), for A/B tests; and its logging.
#ifndef SAFE_RECALL
#define SAFE_RECALL true
#endif
#ifndef RECALL_TRACE
#define RECALL_TRACE false
#endif
// Per-turn state log (bot.cpp): round, champion, chosen state, position, gems, health.
#ifndef STATE_TRACE
#define STATE_TRACE false
#endif
// Safe-recall tunables for test sweeps.
#ifndef RECALL_ENEMY_BUFFER
#define RECALL_ENEMY_BUFFER 2
#endif
#ifndef RECALL_BUSH_BONUS
#define RECALL_BUSH_BONUS 3.0f
#endif
// Test opponent: buy an altar lock whenever possible and place it on any unlocked altar in range.
#ifndef TEST_ALTAR_LOCKER
#define TEST_ALTAR_LOCKER false
#endif
// Break enemy altar locks in the tribute state (false: the old behaviour, for A/B tests), and a
// score/lock log for those tests.
#ifndef BREAK_ALTAR_LOCKS
#define BREAK_ALTAR_LOCKS true
#endif
#ifndef SCORE_TRACE
#define SCORE_TRACE false
#endif
// Opening: each champion starts at its own closest camp (false: the old opening, for A/B tests).
#ifndef OPENING_CLOSEST_CAMP
#define OPENING_CLOSEST_CAMP true
#endif
// Each champion picks its own camp (false: the shared team target, for A/B tests).
#ifndef PER_CHAMPION_CAMPS
#define PER_CHAMPION_CAMPS false
#endif
// Per-champion camp tunables for test sweeps.
#ifndef CAMP_TEAMMATE_BONUS
#define CAMP_TEAMMATE_BONUS 6.0f
#endif
#ifndef MAX_CAMP_SHARE_NEAR_SPAWN
#define MAX_CAMP_SHARE_NEAR_SPAWN 0.6f
#endif
// Wait while enemies fight camps (false: attack them at once, the old behaviour, for A/B tests).
#ifndef WAIT_FOR_CAMP_FIGHTS
#define WAIT_FOR_CAMP_FIGHTS true
#endif
// Camp danger limit for test sweeps (constants/state_constants.h kMaxCampDamageShare).
#ifndef MAX_CAMP_DAMAGE_SHARE
#define MAX_CAMP_DAMAGE_SHARE 3.0f
#endif
#ifndef MAX_CAMP_DAMAGE_SHARE_GEM_STARVED
#define MAX_CAMP_DAMAGE_SHARE_GEM_STARVED 1000.0f
#endif
// Pathing tie-break (constants/nav_constants.h kStraightTieBreak).
#ifndef STRAIGHT_TIE_BREAK
#define STRAIGHT_TIE_BREAK false
#endif
// Death tests (constants/state_constants.h): early recall near enemies, carriers avoid fights.
#ifndef RECALL_HP_NEAR_ENEMY
#define RECALL_HP_NEAR_ENEMY 0.35f
#endif
#ifndef CARRIER_AVOID_GEMS
#define CARRIER_AVOID_GEMS 0
#endif
// Idle recall threshold (constants/state_constants.h kIdleRecallHpFraction).
#ifndef IDLE_RECALL_HP
#define IDLE_RECALL_HP 1.0f
#endif
// Farm tunables for test sweeps (constants/state_constants.h).
#ifndef ROUND_DISCOUNT
#define ROUND_DISCOUNT 0.93f
#endif
#ifndef GOLD_WEIGHT
#define GOLD_WEIGHT 0.25f
#endif
#ifndef TRIBUTE_GEMS
#define TRIBUTE_GEMS 40
#endif
// Shop plan (states/shop.h); 13 = emblems then Power shards of the main stat, 0 = Bruiser health only.
#ifndef SHOP_PLAN
#define SHOP_PLAN 13
#endif
// Our altar lock plan (constants/brain_constants.h kAltarLockPlan).
#ifndef ALTAR_LOCK_PLAN
#define ALTAR_LOCK_PLAN 0
#endif
// Dash plan (constants/brain_constants.h kDashPlan).
#ifndef DASH_PLAN
#define DASH_PLAN 2
#endif
// Tribute safety plan (constants/brain_constants.h kTributeSafetyPlan).
#ifndef TRIBUTE_SAFETY_PLAN
#define TRIBUTE_SAFETY_PLAN 0
#endif
// Pursuit plan (constants/brain_constants.h kPursuePlan).
#ifndef PURSUE_PLAN
#define PURSUE_PLAN 0
#endif
// Stay-together farming (constants/state_constants.h kStayTogether).
#ifndef STAY_TOGETHER
#define STAY_TOGETHER false
#endif
// Bunch-up-near-enemies mode (constants/state_constants.h kBunchMode).
#ifndef BUNCH_MODE
#define BUNCH_MODE 0
#endif
#ifndef RALLY_RANGE
#define RALLY_RANGE 15
#endif
#ifndef RALLY_RADIUS
#define RALLY_RADIUS 3
#endif
#ifndef RALLY_ROUNDS
#define RALLY_ROUNDS 3
#endif
