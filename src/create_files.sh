#!/bin/bash

# Constants
cat > constants/brain_constants.h << 'EOF'
#pragma once
namespace constants {
constexpr int kRecallScore = 100;
constexpr int kAttackScore = 80;
constexpr int kTributeScore = 60;
constexpr int kFarmScore = 40;
constexpr int kDefaultSwitchMargin = 10;
constexpr int kInitFuelReserve = 50000;
}
EOF

cat > constants/state_constants.h << 'EOF'
#pragma once
namespace constants {
constexpr float kRecallHpFraction = 0.35f;
constexpr int kDefaultShardStat = 0;
}
EOF

cat > constants/nav_constants.h << 'EOF'
#pragma once
namespace constants {
constexpr int kMinFuelForPathing = 30000;
}
EOF

cat > constants/draft_constants.h << 'EOF'
#pragma once
namespace constants {
}
EOF

# Debug
cat > debug/log.h << 'EOF'
#pragma once
namespace debug {
constexpr bool kLogging = false;
inline void log(const char*) {}
}
EOF

cat > debug/fuel_meter.h << 'EOF'
#pragma once
#include <cambc.h>
#include "log.h"
namespace debug {
class FuelMeter {
public:
    explicit FuelMeter(const char*) {}
    ~FuelMeter() {}
};
}
EOF

cat > debug/overlay.h << 'EOF'
#pragma once
#include <cambc.h>
namespace debug {
inline void draw_dot(cambc::vec2) {}
inline void draw_line(cambc::vec2, cambc::vec2) {}
}
EOF

# Core
cat > core/rng.h << 'EOF'
#pragma once
#include <cambc.h>
#include <cstdint>
#include <random>
inline std::uint32_t random_seed() {
    const auto spawn = cambc::map::spawn_region(cambc::team::ally).positions()[0];
    std::uint32_t seed = spawn.x * 73856093u ^ spawn.y * 19349663u;
    if (!cambc::is_draft()) {
        const auto start = cambc::self().position();
        seed ^= start.x * 83492791u ^ start.y * 2654435761u;
    }
    return seed;
}
inline unsigned random_number(unsigned limit) {
    static std::mt19937 random{random_seed()};
    return std::uniform_int_distribution<unsigned>{0, limit - 1}(random);
}
EOF

cat > core/geometry.h << 'EOF'
#pragma once
#include <cambc.h>
using cambc::vec2;
inline int distance(vec2 a, vec2 b) {
    return static_cast<int>(a.chebyshev_distance(b));
}
inline cambc::bitboard tiles_around(vec2 position, int radius) {
    cambc::bitboard tiles;
    tiles.clear();
    tiles.draw_chebyshev(position, radius);
    return tiles;
}
EOF

cat > core/match_data.h << 'EOF'
#pragma once
#include <cambc.h>
#include <vector>
#include <variant>
struct camp_group_alive {};
struct camp_group_reward_available {};
struct camp_group_respawning { std::int32_t timer; };
using camp_group_state = std::variant<camp_group_alive, camp_group_reward_available, camp_group_respawning>;
struct CachedCampGroup {
    cambc::handle_camp_group_t handle;
    cambc::bitboard spawn_tiles;
    std::uint32_t gold_value;
    std::uint32_t gem_value;
    std::int32_t max_respawn_timer;
};
struct MatchData {
    std::vector<CachedCampGroup> camp_groups;
    cambc::bitboard static_blockers;
    bool initialized = false;
};
EOF

cat > core/bot_memory.h << 'EOF'
#pragma once
#include <variant>
enum class State;
struct RecallMemory {};
struct AttackMemory {};
struct TributeMemory {};
struct FarmMemory {};
struct BotMemory {
    std::variant<std::monostate, RecallMemory, AttackMemory, TributeMemory, FarmMemory> current_state;
    int init_stage = 0;
};
EOF

cat > core/context.h << 'EOF'
#pragma once
#include <cambc.h>
#include <optional>
struct ChampionProfile;
struct TurnContext {
    std::uint32_t round_number;
    const ChampionProfile& profile;
    struct Enemies { std::optional<std::vector<cambc::champion>> visible_champions; } enemies;
    struct Camps { std::optional<std::vector<cambc::camp>> visible_camps; } camps;
    struct Resources { std::optional<std::uint32_t> gold; std::optional<std::uint32_t> gems; } resources;
    struct Stats { std::optional<std::int32_t> health; std::optional<std::int32_t> max_health; } stats;
    cambc::vec2 position;
    TurnContext(const ChampionProfile& profile_ref);
};
EOF

cat > core/context.cpp << 'EOF'
#include "context.h"
TurnContext::TurnContext(const ChampionProfile& profile_ref)
    : round_number(cambc::round_number()), profile(profile_ref), position(cambc::self().position()) {}
EOF

cat > core/map_snapshot_shared.h << 'EOF'
#pragma once
#include <cambc.h>
EOF

cat > core/map_snapshot_shared.cpp << 'EOF'
#include "map_snapshot_shared.h"
EOF

# Champions
cat > champions/champion_profile.h << 'EOF'
#pragma once
#include <cambc.h>
#include <functional>
#include <vector>
class Skill;
struct ChampionProfile {
    bool is_ranged;
    std::vector<Skill*> skills;
    bool linked_pair;
    std::function<void()> special_hook = []{};
};
EOF

# Skills
cat > skills/skill.h << 'EOF'
#pragma once
#include <cambc.h>
struct TurnContext;
class Skill {
public:
    virtual ~Skill() = default;
    virtual bool can_use(const TurnContext& ctx) = 0;
    virtual void use(TurnContext& ctx) = 0;
};
EOF

# States
cat > states/state_module.h << 'EOF'
#pragma once
#include <cambc.h>
#include "../brain/state.h"
#include "../core/bot_memory.h"
struct TurnContext;
enum class StateResult { Continue, Done, Failed };
struct StateModule {
    virtual ~StateModule() = default;
    virtual int score(const TurnContext& ctx) = 0;
    virtual StateResult execute(TurnContext& ctx, BotMemory& memory) = 0;
    virtual void on_enter(TurnContext& ctx, BotMemory& memory) {}
    virtual void on_exit(TurnContext& ctx, BotMemory& memory) {}
    virtual int switch_margin() const { return 10; }
};
EOF

# Brain
cat > brain/state.h << 'EOF'
#pragma once
enum class State { Recall, Attack, Tribute, Farm, None };
EOF

cat > brain/state_selector.h << 'EOF'
#pragma once
#include <cambc.h>
#include <vector>
#include "state.h"
#include "../states/state_module.h"
#include "../core/bot_memory.h"
#include "../constants/brain_constants.h"
struct TurnContext;
class StateSelector {
public:
    void add_state(StateModule* module, State state_enum);
    void select_and_execute(TurnContext& ctx, BotMemory& memory);
private:
    StateModule* get_module_for_state(State state);
    struct StateEntry { StateModule* module; State state_enum; };
    std::vector<StateEntry> states_;
};
EOF

# Comms
cat > comms/draft_memory.h << 'EOF'
#pragma once
#include <array>
#include <cstdint>
struct DraftMemory {
    std::uint32_t version = 1;
    std::array<std::uint8_t, 1020> reserved{};
    std::array<std::uint8_t, 1024>& buffer() {
        return reinterpret_cast<std::array<std::uint8_t, 1024>&>(*this);
    }
};
EOF

cat > comms/shared_memory.h << 'EOF'
#pragma once
#include <array>
#include <cstdint>
struct SharedMemory {
    std::array<std::uint8_t, 8> data{};
    std::array<std::uint8_t, 8>& buffer() { return data; }
};
EOF

# Draft
cat > draft/draft.h << 'EOF'
#pragma once
#include <cambc.h>
void play_draft();
EOF

# Init
cat > init/initializer.h << 'EOF'
#pragma once
#include <cambc.h>
struct MatchData;
struct BotMemory;
using StageFunc = bool (*)(MatchData&);
void run_init_stages(MatchData& match_data, BotMemory& memory);
EOF

# Nav
cat > nav/blocked_tiles.h << 'EOF'
#pragma once
#include <cambc.h>
cambc::bitboard blocked_tiles();
EOF

cat > nav/pathfinding.h << 'EOF'
#pragma once
#include <cambc.h>
#include "../core/geometry.h"
bool move_towards_closest(cambc::bitboard goals);
EOF

cat > nav/movement.h << 'EOF'
#pragma once
#include <cambc.h>
EOF

echo "Files created"
