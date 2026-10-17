#pragma once
#include <cstdint>
#include "test_hooks.h"
namespace constants {
// Recall below this fraction of max health (only once nothing is hitting us).
constexpr float kRecallHpFraction = 0.35f;
// Tribute when carrying this many gems, or any gems when an altar is this close.
constexpr std::uint32_t kTributeGems = 40;
constexpr int kTributeIfAltarWithin = 8;
// Camp value: (gems + gold x kGoldWeight) x kRoundDiscount ^ (walk + kill + capture rounds).
constexpr float kRoundDiscount = 0.97f;
constexpr float kGoldWeight = 0.25f;
// Only camps within this walk are considered (the search stops there).
constexpr int kCampSearchRange = 60;
// Switch farm target only for a camp worth this much more than the current one.
constexpr float kSwitchFactor = 1.25f;
// Skip a camp if the damage it would deal while we kill it, shared among us and the allies near
// us, exceeds this fraction of our health.
constexpr float kMaxCampDamageShare = 0.6f;
// Allies within this distance count as helping with a camp.
constexpr int kHelpRadius = 5;
// Rough ability damage per round as a fraction of focus (about 1.5-2x focus every 8 rounds).
constexpr float kAbilityDpsPerFocus = 0.2f;
// Safe recall (nav/safe_recall.cpp) on; false: recall where we stand if nothing hit us since our
// last turn, else walk home (the old behaviour).
constexpr bool kSafeRecall = SAFE_RECALL;
// Safe recall (nav/safe_recall.cpp): before recalling, get out of reach of what can hit us.
// Enemy champions threaten tiles within their attack/spell reach plus this buffer; awake camps
// within their attack range plus kRecallCampBuffer.
constexpr int kRecallEnemyBuffer = RECALL_ENEMY_BUFFER;   // 2 unless a test build overrides
constexpr int kRecallCampBuffer = 0;
// Look for a safe tile at most this many steps away; none found: walk home as before.
constexpr int kRecallSearchSteps = 8;
// Only safe tiles at most this many steps beyond the nearest safe tile are scored (each tile
// scored costs ~300 fuel; a step costs as much as the tiebreaks can gain back per step).
constexpr int kRecallExtraSteps = 2;
// Tiebreak weights between safe tiles (higher score wins): per step to get there, per tile of
// walk home, per tile from the nearest visible teammate, per tile from the other visible enemies
// (counted up to kRecallEnemyDistCap), and a bonus for a bush (bushes hide us from champions
// outside them; camps see through them).
constexpr float kRecallWeightSteps = 1.0f;
constexpr float kRecallWeightHome = 0.5f;
constexpr float kRecallWeightFriends = 0.25f;
constexpr float kRecallWeightEnemies = 0.25f;
constexpr int kRecallEnemyDistCap = 12;
constexpr float kRecallBushBonus = RECALL_BUSH_BONUS;   // 3
// Opening (to-do #3): at match start each champion farms the camp group with the shortest walk
// from where it stands, until that group is cleared (it starts respawning); then normal farming.
// kOpeningMaxRounds is a safety cap in case the group is never cleared.
constexpr bool kOpeningClosestCamp = OPENING_CLOSEST_CAMP;
constexpr std::uint32_t kOpeningMaxRounds = 200;
}
