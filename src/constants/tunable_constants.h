#pragma once

#include <cstdint>

// Strategy parameters intended for benchmark or optimizer tuning.
// Keep game rules, API limits, serialization layouts, and algorithm bounds elsewhere.
namespace tunable_constants {

// Economy and long-horizon value estimates.
inline constexpr float kGoldWeight = 0.2f;
inline constexpr float kRoundDiscount = 0.93f;
inline constexpr float kRespawnWalkFactor = 1.0f;
inline constexpr std::uint32_t kShopRecallGoldThreshold = 400;
inline constexpr std::uint32_t kPrismTributeShopGemThreshold = 600;
inline constexpr std::uint32_t kGoldWeightStopRound = 1600;
inline constexpr std::uint32_t kGoldWeightStopEnemySubmittedGems = 800;

// Farming and camp selection.
inline constexpr int kCampSearchRange = 10000;
inline constexpr std::uint32_t kBlockedSkipRounds = 10;
inline constexpr std::uint32_t kRewardBeliefRounds = 20;
inline constexpr int kHarvestNearRadius = 3;
inline constexpr int kPrismTetherRadius = 3;
inline constexpr float kLowHealthRecallFraction = 0.35f;
inline constexpr bool kRangedWalkToPriority = false;
inline constexpr int kEnemySpawnAvoidanceRadius = 1;
inline constexpr int kHeartOfRuinMinCampDistance = 9;
inline constexpr std::uint32_t kCaptureYieldRounds = 5;
inline constexpr float kAllyNearbyRadius = 7.0f;
inline constexpr std::uint32_t kPrismAloneRecallRounds = 6;
inline constexpr std::uint32_t kPrismRespawnRegroupLead = 8;
inline constexpr std::uint32_t kRespawnWaitWindow = 4;
inline constexpr float kPrismVisionDistance = 10.0f;
inline constexpr float kFinalTributePreserveHealthFraction = 0.6f;
inline constexpr std::uint32_t kFinalTributePreserveTurns = 25;
inline constexpr float kFinalTributeCombatDistance = 3.0f;
inline constexpr float kPrismFinalTributeCombatDistance = 99.0f;
inline constexpr float kFinalTributeRecallDistance = 5.0f;
inline constexpr float kPrismFinalTributeRecallDistance = 94.0f;
inline constexpr int kDashMinimumProgress = 3;

// Combat decisions.
inline constexpr float kDefaultEngageDistance = 3.0f;
inline constexpr float kPrismEngageDistance = 99.0f;
inline constexpr float kPrismSubmitEngageDistance = 2.0f;
inline constexpr std::uint32_t kPursuitLimit = 5;

// Tribute timing and thresholds.
inline constexpr std::uint32_t kTributeGemThreshold = 1;
inline constexpr std::uint32_t kStaleGemRounds = 160;
inline constexpr std::uint32_t kLateFinalTributeRound = 1800;

// Lower rank is preferred when selecting a camp target.
inline constexpr int kElkCampPriority = 1;
inline constexpr int kGolemCampPriority = 2;
inline constexpr int kWolfCampPriority = 3;
inline constexpr int kMonolithCampPriority = 4;
inline constexpr int kResourceClusterCampPriority = 5;

} // namespace tunable_constants
