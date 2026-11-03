#pragma once
#include "../core/bot_memory.h"
#include <optional>
struct ChampionProfile;
struct MatchData;
void farm_turn(const ChampionProfile& profile, MatchData& match, BotMemory& memory,
               std::optional<cambc::vec2> coordinated_target = std::nullopt,
               cambc::vec2* selected_target = nullptr);
bool handle_cleared_coordinated_target(MatchData& match, BotMemory& memory,
                                       cambc::vec2 target, cambc::vec2 prism_position);
bool retreat_to_spawn(BotMemory& memory, MatchData& match, bool include_enemy_threats,
                      bool include_camp_threats = true, bool camp_reward_shop_recall = false,
                      bool visible_enemy_threat = false);
bool recall_turn(MatchData& match, BotMemory& memory, bool force_recall = false,
                 float active_fight_distance = 5.0f);
