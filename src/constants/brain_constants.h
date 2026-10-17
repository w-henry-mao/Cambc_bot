#pragma once
#include "test_hooks.h"
namespace constants {
constexpr int kRecallScore = RECALL_SCORE;   // test builds may override
constexpr int kAttackScore = 80;
// Attack state score while a teammate's fight call is live and no enemy is in reach yet (the walk
// to the called target). Recall (kRecallScore) still wins over it.
constexpr int kTeamFightCallScore = 80;
constexpr int kTributeScore = 60;
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
constexpr int kInitFuelReserve = 50000;
}
