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
constexpr int kInitFuelReserve = 50000;
}
