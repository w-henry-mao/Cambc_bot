#pragma once
#include <cstddef>
#include <cstdint>
#include "test_hooks.h"

namespace constants {
constexpr float kTwoVOneAllyMinHealth = 0.40f;
constexpr float kTwoVOneEnemyMaxHealth = 0.70f;
constexpr int kDuelRoundCap = 60;
constexpr float kDurabilityDamageTakenMult = 0.9f;
constexpr int kCampAttackRange = 3;
constexpr std::uint64_t kFastSimFuelBudget = FAST_SIM_FUEL_BUDGET;
constexpr std::uint64_t kFullFuelReserve = 25000;
constexpr bool kCombatTrace = COMBAT_TRACE;
constexpr bool kDuelMode = DUEL_MODE;
constexpr int kDuelFighterType = DUEL_FIGHTER_TYPE;
constexpr bool kVerifyPathCache = VERIFY_PATH_CACHE;
constexpr bool kPathCacheEnabled = PATH_CACHE_ENABLED;
constexpr int kPathCacheSlots = 4;
constexpr std::size_t kPathCachePoints = 8;
constexpr std::uint32_t kPathCacheRounds = 60;
constexpr float kTeamFightMargin = 1.3f;
constexpr bool kTeamFightQuick = true;
constexpr float kTeamFightSupportWeight = 0.5f;
constexpr int kTeamFightRadius = 8;
constexpr int kTeamFightRoundCap = 40;
constexpr int kTeamFightCallRounds = TEAM_FIGHT_CALL_ROUNDS;
constexpr std::size_t kAllyPositionByte = 4;
constexpr bool kTeamFightSharedAllies = true;
constexpr int kAllyPositionMaxAge = ALLY_POSITION_MAX_AGE;
constexpr int kTeamFightSharedRadius = TEAM_FIGHT_SHARED_RADIUS;
constexpr bool kTeamFightTestCaller = TEAM_FIGHT_TEST_CALLER;
constexpr int kTeamFightTestCallEvery = TEAM_FIGHT_TEST_CALL_EVERY;
constexpr bool kTeamFightTrace = TEAM_FIGHT_TRACE;
constexpr int kRallyRounds = RALLY_ROUNDS;
}
