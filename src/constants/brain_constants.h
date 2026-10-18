#pragma once
#include <cstdint>
#include "test_hooks.h"
namespace constants {
constexpr int kRecallScore = RECALL_SCORE;   // test builds may override
constexpr int kAttackScore = 80;
// Attack state score while a teammate's fight call is live and no enemy is in reach yet (the walk
// to the called target). Recall (kRecallScore) still wins over it.
constexpr int kTeamFightCallScore = 80;
constexpr int kTributeScore = 60;
// Shop trip (states/recall.cpp): recall score when the gold we carry finishes our next planned
// emblem (states/shop.h) and we are between jobs: no enemy in reach, not hit since last turn, not
// hitting, capturing or about to capture our camp, and no gems to bank first.
constexpr bool kShopRecall = SHOP_RECALL;
constexpr int kShopRecallScore = 100;
constexpr int kFarmScore = 40;
constexpr int kDefaultSwitchMargin = 10;
// Camp fights (to-do #5): an enemy fighting a camp (an awake camp within its reach) is attacked
// only once that camp group is at or below kWaitCampHealthFraction of its health (or dead), or
// while we are not the champion closest to the camp (camps hit the closest). Until then we hold
// position, stepping back if we are the closest. A teammate's fight call overrides this.
constexpr bool kWaitForCampFights = WAIT_FOR_CAMP_FIGHTS;
constexpr float kWaitCampHealthFraction = 0.3f;
// Our altar locks (skills/altar_locks.h): plan 0 never, 1 any unlocked altar in reach, 2 where an
// enemy is tributing, 3 both. Placement reach is 2 tiles (game rule).
constexpr int kAltarLockPlan = ALTAR_LOCK_PLAN;
constexpr int kAltarLockRange = 2;
// Dash (skills/dash_move.h): plan 0 never, 1 to close in on the attack target, 2 toward the safe
// tile when retreating to recall, 3 both.
constexpr int kDashPlan = DASH_PLAN;   // 2: escape only (tested best; README)
// Tribute safety (brain/channel_safety.h): 0 never cancel, 1 cancel when hit, 2 cancel when the
// enemies in reach could kill us before the tribute completes.
constexpr int kTributeSafetyPlan = TRIBUTE_SAFETY_PLAN;
// Pursuit (states/attack.cpp): 0 off; 1 a recalling/tributing enemy in reach is attacked first;
// 2 also chase one within kPursueRange carrying at least kPursueMinGems while we have at least
// kPursueMinHealth of our health.
constexpr int kPursuePlan = PURSUE_PLAN;
constexpr int kPursueRange = 8;
constexpr std::uint32_t kPursueMinGems = 15;
constexpr float kPursueMinHealth = 0.5f;
constexpr int kInitFuelReserve = 50000;
// End-of-turn setup steps (init/deferred_setup.h) run only while this much fuel is left; the
// largest step (the walk home) costs ~25k on a 100x100 map, so a turn never reaches its limit
// because of them.
constexpr int kDeferredSetupReserve = 35000;
}
