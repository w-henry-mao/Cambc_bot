#pragma once
#include <cstdint>
#include "test_hooks.h"

namespace constants {
constexpr int kRecallScore = RECALL_SCORE;
constexpr int kAttackScore = 80;
constexpr int kTeamFightCallScore = 80;
constexpr int kTributeScore = 60;
constexpr bool kShopRecall = SHOP_RECALL;
constexpr int kShopRecallScore = 100;
constexpr int kFarmScore = 40;
constexpr int kDefaultSwitchMargin = 10;
constexpr bool kWaitForCampFights = WAIT_FOR_CAMP_FIGHTS;
constexpr float kWaitCampHealthFraction = 0.3f;
constexpr int kAltarLockPlan = ALTAR_LOCK_PLAN;
constexpr int kAltarLockRange = 2;
constexpr int kDashPlan = DASH_PLAN;
constexpr int kTributeSafetyPlan = TRIBUTE_SAFETY_PLAN;
constexpr int kPursuePlan = PURSUE_PLAN;
constexpr int kPursueRange = 8;
constexpr std::uint32_t kPursueMinGems = 15;
constexpr float kPursueMinHealth = 0.5f;
constexpr int kInitFuelReserve = 50000;
constexpr int kDeferredSetupReserve = 35000;
}
