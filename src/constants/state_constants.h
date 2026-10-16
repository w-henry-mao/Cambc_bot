#pragma once
#include <cstdint>
namespace constants {
// Recall below this fraction of max health (only once nothing is hitting us).
constexpr float kRecallHpFraction = 0.35f;
// Tribute when carrying this many gems, or any gems when an altar is this close.
constexpr std::uint32_t kTributeGems = 40;
constexpr int kTributeIfAltarWithin = 8;
// Camp value: (gems + gold x kGoldWeight) x kRoundDiscount ^ (walk + kill + capture rounds).
constexpr float kRoundDiscount = 0.99f;
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
}
