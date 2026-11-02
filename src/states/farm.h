#pragma once
#include "../core/bot_memory.h"
#include <optional>
struct ChampionProfile;
struct MatchData;
struct FarmMovementStatus {
    bool attempted = false;
    bool succeeded = false;
};
void farm_turn(const ChampionProfile& profile, MatchData& match, BotMemory& memory,
               std::optional<cambc::vec2> coordinated_target = std::nullopt,
               cambc::vec2* selected_target = nullptr,
               FarmMovementStatus* movement_status = nullptr);
bool retreat_to_spawn(BotMemory& memory, MatchData& match, bool include_enemy_threats);
bool recall_turn(MatchData& match, BotMemory& memory, bool force_recall = false,
                 float active_fight_distance = 5.0f);
