#pragma once
#include <cambc.h>
#include <optional>
struct ChampionProfile;
struct MatchData;
struct TurnContext {
    std::uint32_t round_number;
    const ChampionProfile& profile;
    struct Enemies { std::optional<std::vector<cambc::champion>> visible_champions; } enemies;
    struct Camps { std::optional<std::vector<cambc::camp>> visible_camps; } camps;
    struct Resources { std::optional<std::uint32_t> gold; std::optional<std::uint32_t> gems; } resources;
    struct Stats { std::optional<std::int32_t> health; std::optional<std::int32_t> max_health; } stats;
    cambc::vec2 position;
    MatchData* match_data = nullptr;
    TurnContext(const ChampionProfile& profile_ref);
};
