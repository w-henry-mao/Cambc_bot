#pragma once
#include <cstddef>
#include <cstdint>
#include "test_hooks.h"
namespace constants {
// Two-against-one quick check (team's rule): favourable when neither ally is below this
// fraction of max health and the enemy is not above the other.
constexpr float kTwoVOneAllyMinHealth = 0.40f;
constexpr float kTwoVOneEnemyMaxHealth = 0.70f;
// Rounds simulated before a duel counts as unresolved.
constexpr int kDuelRoundCap = 60;
// Damage reduction from a Durability emblem (docs/GameRules.md; not yet measured).
constexpr float kDurabilityDamageTakenMult = 0.9f;
// Camps count as fighting when this close (their attack range).
constexpr int kCampAttackRange = 3;
// Fuel the fast fight prediction may spend simulating before it answers "unresolved".
constexpr std::uint64_t kFastSimFuelBudget = FAST_SIM_FUEL_BUDGET;   // test builds may override
// Fuel the full prediction leaves for the rest of the turn.
constexpr std::uint64_t kFullFuelReserve = 25000;

// Fight-calculator logging (FIGHT_PRED / FIGHT_END lines). Test builds turn it on.
constexpr bool kCombatTrace = COMBAT_TRACE;
// Duel test mode: only the champion of this type plays; it walks to the map centre and fights
// the first enemy it meets with the normal attack code. Test builds only.
constexpr bool kDuelMode = DUEL_MODE;
constexpr int kDuelFighterType = DUEL_FIGHTER_TYPE;
// Test builds: on every path-cache hit, also recompute and log whether the answers agree.
constexpr bool kVerifyPathCache = VERIFY_PATH_CACHE;
// Stored fast-prediction paths, and how many rounds one stays usable.
constexpr bool kPathCacheEnabled = PATH_CACHE_ENABLED;   // test builds may switch it off to compare
constexpr int kPathCacheSlots = 4;
// Path points recorded per prediction (our next turns). Real fights rarely stay on the predicted
// path for long, so later points are seldom reused and only cost fuel to record.
constexpr std::size_t kPathCachePoints = 8;
constexpr std::uint32_t kPathCacheRounds = 60;

// Team-fight estimate (combat/sdk_team_fight.h).
// Fight only if we still win when the enemy deals this many times its estimated damage; Avoid
// only if they still win when we deal this many times ours.
constexpr float kTeamFightMargin = 1.3f;
// Which estimate estimate_team_fight_here() uses: the event-driven quick version (true) or the
// round-by-round one (false). In 20 test matches they agreed on 9 in 10 calls; the quick one
// costs about half the fuel.
constexpr bool kTeamFightQuick = true;
// Quick version: weight of heal and shield relief (1 = every heal/shield fully used). Real ones
// land only in part (below 80% health, in range, no overheal, shields expire unused); 0.5 ranked
// real fights best in two 391-match test batches (tools/team_resim.cpp).
constexpr float kTeamFightSupportWeight = 0.5f;
// Champions this close to us (and visible) count as in the fight.
constexpr int kTeamFightRadius = 8;
// Rounds played out before the estimate gives up (Unsure).
constexpr int kTeamFightRoundCap = 40;
// Fight calls live in the shared header word (comms/shared_memory.h has the whole layout). A call
// older than kTeamFightCallRounds rounds is ignored.
constexpr int kTeamFightCallRounds = TEAM_FIGHT_CALL_ROUNDS;   // 2 unless a test build overrides
// Teammate positions in shared memory (comms/ally_positions.h).
constexpr std::size_t kAllyPositionByte = 4;   // comms/shared_memory.h team_comms::kPositionByte
// The estimate also counts teammates we cannot see whose last report is at most this many rounds
// old and within kTeamFightSharedRadius of us (they walk in, using the stats we last saw them
// with, or their base stats).
constexpr bool kTeamFightSharedAllies = true;
constexpr int kAllyPositionMaxAge = ALLY_POSITION_MAX_AGE;   // 3
constexpr int kTeamFightSharedRadius = TEAM_FIGHT_SHARED_RADIUS;   // 8
// Test builds: brain/team_fight_call.cpp calls a fight whenever the estimate says Fight.
constexpr bool kTeamFightTestCaller = TEAM_FIGHT_TEST_CALLER;
constexpr int kTeamFightTestCallEvery = TEAM_FIGHT_TEST_CALL_EVERY;
// Test builds: log TEAM_PRED / TEAM_END lines to check the estimate against real fights.
constexpr bool kTeamFightTrace = TEAM_FIGHT_TRACE;
}
