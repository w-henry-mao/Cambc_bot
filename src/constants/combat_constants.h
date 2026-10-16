#pragma once
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
}
