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
// One champion collects a camp's reward while the others move on (false: everyone at the camp
// stands on it and the reward is split, the old behaviour, for A/B tests).
#ifndef TAVI_FOCUS
#define TAVI_FOCUS false
#endif
#ifndef COLLECTOR_PREFER_RANGED
#define COLLECTOR_PREFER_RANGED false
#endif
#ifndef COLLECTOR_RADIUS
#define COLLECTOR_RADIUS 3
#endif
#ifndef SINGLE_COLLECTOR
#define SINGLE_COLLECTOR true
#endif
// While a camp group respawns, wait next to its spawn tiles rather than on them (false: walk onto
// them, the old behaviour that can hold the group's respawn forever, for A/B tests).
#ifndef WAIT_BESIDE_SPAWN
#define WAIT_BESIDE_SPAWN true
#endif
// Recall to shop once our gold finishes the next planned emblem, between jobs (false: shop only when
// on spawn for other reasons, the old behaviour, for A/B tests).
#ifndef SHOP_RECALL
#define SHOP_RECALL true
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
// Two farm groups (states/farm.cpp) and avoiding outnumbered fights (states/attack.cpp).
#ifndef SPLIT_GROUPS
#define SPLIT_GROUPS false
#endif
#ifndef AVOID_OUTNUMBERED
#define AVOID_OUTNUMBERED false
#endif
// Camp danger tests (states/farm.cpp): danger limit for camps picked alone, and counting
// teammates actually near a camp as help in that choice.
#ifndef SOLO_DAMAGE_SHARE
#define SOLO_DAMAGE_SHARE MAX_CAMP_DAMAGE_SHARE
#endif
#ifndef REAL_BACKUP
#define REAL_BACKUP false
#endif
// Remember enemy altar locks and bank at unlocked altars (states/tribute.cpp).
#ifndef ALTAR_MEMORY
#define ALTAR_MEMORY false
#endif
// Recall to shop once we hold this much gold (0 = off, 1 = enough for the next emblem; states/recall.cpp).
#ifndef SHOP_RECALL_GOLD
#define SHOP_RECALL_GOLD 0
#endif
// Capture stayer (states/farm.cpp): one champion captures a cleared camp group while the rest move
// on. 0 = off, 1 = closest to the group, 2 = lowest health fraction, 3 = lowest damage.
#ifndef CAPTURE_STAYER
#define CAPTURE_STAYER 0
#endif
// Fight value check (states/attack.cpp): 0 = off, 1 = before starting a fight, 2 = every turn.
#ifndef FIGHT_EV
#define FIGHT_EV 0
#endif
// Recall below this fraction of max health (constants/state_constants.h kRecallHpFraction).
#ifndef RECALL_HP
#define RECALL_HP 0.35f
#endif
// Champions per camp by strength (states/farm.cpp): a camp needs enough of us to kill it in this
// many rounds; once that many teammates are at the team target, others farm elsewhere (0 = off).
#ifndef CAMP_CROWD_ROUNDS
#define CAMP_CROWD_ROUNDS 0
#endif
// Round discount on monolith-only maps (constants/state_constants.h kStarvedRoundDiscount).
#ifndef STARVED_ROUND_DISCOUNT
#define STARVED_ROUND_DISCOUNT ROUND_DISCOUNT
#endif
// Test builds: print the map's camp groups once (bot.cpp), for map classification work.
#ifndef MAP_DUMP
#define MAP_DUMP false
#endif
// Camp danger with camps killed weakest first (states/farm.cpp camp_value; init/map_snapshot.cpp).
#ifndef SEQ_CAMP_DAMAGE
#define SEQ_CAMP_DAMAGE false
#endif
// Wards (skills/wards.cpp): 0 off, 1 map centre, 2 our farm camp, 3 ahead toward the enemy spawn.
#ifndef WARD_PLAN
#define WARD_PLAN 0
#endif
// Early retreat (states/farm.cpp): enemies at our farm camp outnumber us there -> back off home.
#ifndef EARLY_RETREAT
#define EARLY_RETREAT false
#endif
// Test builds: per-turn fight log in the attack state (states/attack.cpp).
#ifndef FIGHT_TRACE
#define FIGHT_TRACE false
#endif
// Fight execution tests: focus fire (states/attack.cpp), finish a kill before recalling (recall.cpp).
#ifndef FOCUS_FIRE
#define FOCUS_FIRE false
#endif
#ifndef FINISH_KILL
#define FINISH_KILL false
#endif
// Use abilities / melee attacks on the nearest enemy while walking away to recall (states/recall.cpp).
#ifndef RETREAT_FIRE
#define RETREAT_FIRE false
#endif
// Idle recall threshold (constants/state_constants.h kIdleRecallHpFraction).
#ifndef IDLE_RECALL_HP
#define IDLE_RECALL_HP 1.0f
#endif
// Farm tunables for test sweeps (constants/state_constants.h).
#ifndef ROUND_DISCOUNT
#define ROUND_DISCOUNT 0.93f
#endif
#ifndef REACH_CAMP
#define REACH_CAMP false
#endif
#ifndef ROUTE_DEPTH
#define ROUTE_DEPTH 1
#endif
#ifndef ROUTE_LOOKAHEAD
#define ROUTE_LOOKAHEAD 0
#endif
#ifndef COMMIT_TARGET
#define COMMIT_TARGET 0.0f
#endif
#ifndef GOLD_DECAY_PLAN
#define GOLD_DECAY_PLAN 0
#endif
#ifndef GOLD_DECAY_SCALE
#define GOLD_DECAY_SCALE 600.0f
#endif
#ifndef GOLD_DECAY_START
#define GOLD_DECAY_START 1.0f
#endif
#ifndef GOLD_DECAY_ROUNDS
#define GOLD_DECAY_ROUNDS 500.0f
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
