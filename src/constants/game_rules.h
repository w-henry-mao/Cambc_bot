#pragma once

#include <cstdint>

// Fixed engine rules used by the bot's planning models. These are not strategy knobs.
namespace game_rules {

inline constexpr std::uint32_t kChampionRespawnRounds = 60;
inline constexpr std::uint32_t kAbilityCooldownRounds = 8;
inline constexpr int kDashRange = 2;
inline constexpr int kCaptureRounds = 5;
inline constexpr int kCaptureRoundsWithEmblem = 3;
inline constexpr std::uint32_t kVictoryGemThreshold = 1000;
inline constexpr std::uint32_t kGemsPerTributeTurn = 10;
inline constexpr std::uint32_t kMinimumTributeRounds = 1;
inline constexpr std::uint32_t kTributeEmblemDurationPercent = 67;
inline constexpr std::uint32_t kPercentScale = 100;

} // namespace game_rules
