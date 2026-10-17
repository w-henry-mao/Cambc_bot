#pragma once
#include <cstdint>
#include "test_hooks.h"
namespace constants {
// Recall below this fraction of max health (only once nothing is hitting us).
constexpr float kRecallHpFraction = 0.35f;
// With nothing to farm, recall below this fraction of max health (1 = whenever hurt; 0 = never).
constexpr float kIdleRecallHpFraction = IDLE_RECALL_HP;
// Tribute when carrying this many gems, or any gems when an altar is this close.
constexpr std::uint32_t kTributeGems = TRIBUTE_GEMS;   // 40 unless a test build overrides
constexpr int kTributeIfAltarWithin = 8;
// Camp value: (gems + gold x kGoldWeight) x kRoundDiscount ^ (walk + kill + capture rounds).
constexpr float kRoundDiscount = ROUND_DISCOUNT;   // 0.93 unless a test build overrides (README, Farming)
constexpr float kGoldWeight = GOLD_WEIGHT;         // 0.25 unless a test build overrides
// Only camps within this walk are considered (the search stops there).
constexpr int kCampSearchRange = 60;
// Switch farm target only for a camp worth this much more than the current one.
constexpr float kSwitchFactor = 1.25f;
// Skip a camp if the damage it would deal while we kill it, shared among us and the allies near
// us, exceeds this fraction of our health.
constexpr float kMaxCampDamageShare = MAX_CAMP_DAMAGE_SHARE;   // test builds may override
// When no gem group on the map passes kMaxCampDamageShare for a single champion (stampede, contest:
// the only gems are in monoliths), gem groups use this limit instead, so the team keeps at them
// through recall cycles (monoliths keep their damage while we heal).
constexpr float kMaxCampDamageShareGemStarved = MAX_CAMP_DAMAGE_SHARE_GEM_STARVED;
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
// Per-champion camp choice (to-do #4): each champion picks its own best camp group instead of
// following a shared team target. Teammates within kHelpRadius of a group (seen, or from
// position reports) speed the kill, share the damage, and add kCampTeammateBonus each to its
// value (value units: gems + gold x kGoldWeight). Groups within kNearSpawnWalk steps of spawn
// may cost up to kMaxCampDamageShareNearSpawn of our health (we can recall and heal).
constexpr bool kPerChampionCamps = PER_CHAMPION_CAMPS;   // off: lost ~120-200 gems a game in tests (README)
constexpr float kCampTeammateBonus = CAMP_TEAMMATE_BONUS;   // 6 unless a test build overrides
constexpr int kNearSpawnWalk = 15;
constexpr float kMaxCampDamageShareNearSpawn = MAX_CAMP_SHARE_NEAR_SPAWN;   // 0.6 (= the normal limit; 1.0 tested worse)
// Stay together (A/B test): farm only the team target, and when farming walk back to the
// leader (first living champion in draft order) if more than kGroupRadius tiles away.
constexpr bool kStayTogether = STAY_TOGETHER;
constexpr int kGroupRadius = 5;
// Bunch up when enemies are near (A/B test). kBunchMode 1 (rally): a champion seeing an enemy
// within kRallyTrigger broadcasts a rally point at its position; farming teammates within
// kRallyRange walk to within kRallyRadius of it while it is at most kRallyRounds old.
// kBunchMode 2 (local): a farming champion seeing an enemy within kRallyTrigger walks toward its
// nearest visible teammate if that one is more than kRallyRadius away. 0 = off.
constexpr int kBunchMode = BUNCH_MODE;
constexpr int kRallyTrigger = 8;
constexpr int kRallyRange = RALLY_RANGE;   // 15 unless a test build overrides
constexpr int kRallyRadius = RALLY_RADIUS;   // 3
}
