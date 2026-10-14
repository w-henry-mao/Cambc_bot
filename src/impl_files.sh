#!/bin/bash

# Brain state selector
cat > brain/state_selector.cpp << 'EOF'
#include "state_selector.h"
#include "../core/context.h"
#include "../core/bot_memory.h"
#include "../states/state_module.h"
#include "../debug/log.h"
#include <algorithm>
void StateSelector::select_and_execute(TurnContext& ctx, BotMemory& memory) {
    std::vector<std::pair<State, int>> scores;
    for (const auto& entry : states_) {
        int score = entry.module->score(ctx);
        if (score > 0) scores.push_back({entry.state_enum, score});
    }
    if (scores.empty()) return;
    std::sort(scores.begin(), scores.end(),
        [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second > b.second;
            return static_cast<int>(a.first) < static_cast<int>(b.first);
        });
    State current_state = State::None;
    int current_score = 0;
    if (std::holds_alternative<RecallMemory>(memory.current_state)) {
        current_state = State::Recall; current_score = constants::kRecallScore;
    } else if (std::holds_alternative<AttackMemory>(memory.current_state)) {
        current_state = State::Attack; current_score = constants::kAttackScore;
    } else if (std::holds_alternative<TributeMemory>(memory.current_state)) {
        current_state = State::Tribute; current_score = constants::kTributeScore;
    } else if (std::holds_alternative<FarmMemory>(memory.current_state)) {
        current_state = State::Farm; current_score = constants::kFarmScore;
    }
    State chosen_state = State::None;
    StateModule* chosen_module = nullptr;
    if (current_state == State::None || current_score <= 0) {
        chosen_state = scores[0].first;
        chosen_module = get_module_for_state(chosen_state);
    } else {
        for (const auto& [state, score] : scores) {
            if (state == current_state) continue;
            if (score > current_score + constants::kDefaultSwitchMargin) {
                chosen_state = state;
                chosen_module = get_module_for_state(state);
                break;
            }
        }
        if (chosen_state == State::None) {
            chosen_state = current_state;
            chosen_module = get_module_for_state(current_state);
        }
    }
    if (chosen_module) {
        try {
            chosen_module->execute(ctx, memory);
        } catch (const cambc::error&) {
            debug::log("State execution failed");
        }
    }
}
StateModule* StateSelector::get_module_for_state(State state) {
    for (const auto& entry : states_) {
        if (entry.state_enum == state) return entry.module;
    }
    return nullptr;
}
EOF

# States
cat > states/shop.cpp << 'EOF'
#include <cambc.h>
#include <array>
#include "../constants/state_constants.h"
bool shop_pre_step() {
    auto& me = cambc::self();
    const bool at_spawn = cambc::map::spawn_region(cambc::team::ally).is_set(me.position());
    const bool is_dead = !me.is_alive();
    if (!at_spawn && !is_dead) return false;
    const std::array items{cambc::item_type::power_shard_of_strength,
        cambc::item_type::power_shard_of_focus, cambc::item_type::power_shard_of_health};
    const auto item = items[constants::kDefaultShardStat];
    const unsigned count = me.gold_count() / item.cost();
    if (count && me.can_buy(item, count)) {
        me.buy(item, count);
        return true;
    }
    return false;
}
EOF

cat > states/recall.cpp << 'EOF'
#include <cambc.h>
#include "state_module.h"
#include "../constants/brain_constants.h"
#include "../constants/state_constants.h"
class RecallState : public StateModule {
public:
    int score(const TurnContext& ctx) override {
        auto& me = cambc::self();
        const bool at_spawn = cambc::map::spawn_region(cambc::team::ally).is_set(me.position());
        if (!at_spawn && me.health() < me.max_health() * constants::kRecallHpFraction) {
            return constants::kRecallScore;
        }
        return 0;
    }
    StateResult execute(TurnContext& ctx, BotMemory& memory) override {
        cambc::self().recall();
        return StateResult::Done;
    }
};
static RecallState recall_state_instance;
EOF

cat > states/attack.cpp << 'EOF'
#include <cambc.h>
#include "state_module.h"
#include "../constants/brain_constants.h"
class AttackState : public StateModule {
public:
    int score(const TurnContext& ctx) override {
        auto& me = cambc::self();
        if (!me.can_attack()) return 0;
        for (const auto& enemy : cambc::vision::champions(cambc::team::enemy)) {
            if (enemy.is_visible() && enemy.health() > 0 && me.attack_tiles().is_set(enemy.position())) {
                return constants::kAttackScore;
            }
        }
        return 0;
    }
    StateResult execute(TurnContext& ctx, BotMemory& memory) override {
        auto& me = cambc::self();
        for (const auto& enemy : cambc::vision::champions(cambc::team::enemy)) {
            if (enemy.is_visible() && enemy.health() > 0 && me.attack_tiles().is_set(enemy.position())) {
                me.attack(enemy);
                return StateResult::Continue;
            }
        }
        return StateResult::Done;
    }
};
static AttackState attack_state_instance;
EOF

cat > states/tribute.cpp << 'EOF'
#include <cambc.h>
#include "state_module.h"
#include "../nav/pathfinding.h"
#include "../core/geometry.h"
#include "../constants/brain_constants.h"
class TributeState : public StateModule {
public:
    int score(const TurnContext& ctx) override {
        return cambc::self().gem_count() > 0 ? constants::kTributeScore : 0;
    }
    StateResult execute(TurnContext& ctx, BotMemory& memory) override {
        auto& me = cambc::self();
        for (auto altar : cambc::map::altars().positions()) {
            if (distance(me.position(), altar) <= cambc::constants::submit_gems_range && !cambc::vision::has_altar_lock(altar)) {
                me.submit_gems(altar);
                return StateResult::Done;
            }
        }
        cambc::bitboard goals;
        goals.clear();
        for (auto altar : cambc::map::altars().positions()) {
            if (!cambc::vision::has_altar_lock(altar)) {
                goals |= tiles_around(altar, cambc::constants::submit_gems_range);
            }
        }
        if (move_towards_closest(goals)) return StateResult::Continue;
        return StateResult::Failed;
    }
};
static TributeState tribute_state_instance;
EOF

cat > states/farm.cpp << 'EOF'
#include <cambc.h>
#include "state_module.h"
#include "../nav/pathfinding.h"
#include "../constants/brain_constants.h"
class FarmState : public StateModule {
public:
    int score(const TurnContext& ctx) override { return constants::kFarmScore; }
    StateResult execute(TurnContext& ctx, BotMemory& memory) override {
        auto& me = cambc::self();
        for (const auto& camp : cambc::vision::camps()) {
            if (me.can_attack() && camp.is_visible() && camp.health() > 0 && me.attack_tiles().is_set(camp.position())) {
                me.attack(camp);
                return StateResult::Continue;
            }
        }
        cambc::bitboard goals;
        goals.clear();
        const auto visible = cambc::vision::visible_tiles();
        for (const auto& group : cambc::map::camp_groups()) {
            const bool group_visible = !(group.spawn_tiles() & visible).is_empty();
            if (group_visible && std::holds_alternative<cambc::camp_group_respawning>(group.state())) continue;
            goals |= group.spawn_tiles();
        }
        if (goals.is_set(me.position())) return StateResult::Continue;
        if (move_towards_closest(goals)) return StateResult::Continue;
        return StateResult::Failed;
    }
};
static FarmState farm_state_instance;
EOF

# Nav
cat > nav/blocked_tiles.cpp << 'EOF'
#include "blocked_tiles.h"
#include "../core/geometry.h"
cambc::bitboard blocked_tiles() {
    auto blocked = cambc::map::walls() | cambc::map::holes() | cambc::map::altars()
        | cambc::map::spawn_region(cambc::team::enemy) | cambc::vision::champion_tiles()
        | cambc::vision::camp_tiles() | cambc::vision::teleport_tiles();
    blocked.set(cambc::self().position(), false);
    return blocked;
}
EOF

cat > nav/pathfinding.cpp << 'EOF'
#include "pathfinding.h"
#include "blocked_tiles.h"
#include "../constants/nav_constants.h"
bool move_towards_closest(cambc::bitboard goals) {
    auto& me = cambc::self();
    if (!me.can_move() || goals.is_set(me.position())) return false;
    if (cambc::fuel_remaining() < constants::kMinFuelForPathing) return false;
    const auto blocked = blocked_tiles();
    goals &= ~blocked | cambc::vision::camp_tiles();
    if (goals.is_empty()) return false;
    if (goals.count() == 1 && !blocked.is_set(goals.positions()[0]))
        return me.move_towards(goals.positions()[0]);
    const auto path_walls = blocked & ~goals;
    cambc::distance_field field;
    cambc::bitboard visited;
    field.compute_to(path_walls, goals, tiles_around(me.position(), 0), visited);
    auto step = me.position();
    for (auto tile : blocked.reachable_tiles(step, me.base_stats().base_move_speed).positions())
        if (field.get(tile) < field.get(step)) step = tile;
    return step != me.position() && me.move_absolute(step);
}
EOF

cat > nav/movement.cpp << 'EOF'
#include "movement.h"
EOF

# Init
cat > init/initializer.cpp << 'EOF'
#include "initializer.h"
#include "../core/match_data.h"
#include "../constants/brain_constants.h"
#include "../debug/fuel_meter.h"
bool stage_map_snapshot(MatchData&);
bool stage_roster(MatchData&);
bool stage_self_setup(MatchData&);
bool stage_draft_memory_read(MatchData&);
static const StageFunc stages[] = {stage_map_snapshot, stage_roster, stage_self_setup, stage_draft_memory_read};
static constexpr int kNumStages = sizeof(stages) / sizeof(stages[0]);
void run_init_stages(MatchData& match_data, BotMemory& memory) {
    if (match_data.initialized) return;
    int stage_index = memory.init_stage > 0 ? memory.init_stage : 1;
    for (; stage_index <= kNumStages; ++stage_index) {
        debug::FuelMeter meter("init_stage");
        bool done = stages[stage_index - 1](match_data);
        if (done) memory.init_stage = stage_index;
        else return;
        if (cambc::fuel_remaining() < constants::kInitFuelReserve) return;
    }
    match_data.initialized = true;
}
EOF

cat > init/map_snapshot.cpp << 'EOF'
#include <cambc.h>
#include "../core/match_data.h"
using cambc::vec2;
bool stage_map_snapshot(MatchData& match_data) {
    for (const auto& group : cambc::map::camp_groups()) {
        CachedCampGroup cached;
        cached.handle = group;
        cached.spawn_tiles = group.spawn_tiles();
        cached.gold_value = group.gold_value();
        cached.gem_value = group.gem_value();
        cached.max_respawn_timer = group.max_respawn_timer();
        match_data.camp_groups.push_back(cached);
    }
    match_data.static_blockers = cambc::map::walls() | cambc::map::holes()
        | cambc::map::altars() | cambc::map::spawn_region(cambc::team::enemy);
    return true;
}
EOF

cat > init/roster.cpp << 'EOF'
#include <cambc.h>
#include "../core/match_data.h"
bool stage_roster(MatchData& match_data) { return true; }
EOF

cat > init/self_setup.cpp << 'EOF'
#include <cambc.h>
#include "../core/match_data.h"
#include "../champions/registry.cpp"
bool stage_self_setup(MatchData& match_data) {
    if (cambc::is_draft()) return true;
    return true;
}
EOF

cat > init/draft_memory_read.cpp << 'EOF'
#include <cambc.h>
#include "../core/match_data.h"
#include "../comms/draft_memory.h"
bool stage_draft_memory_read(MatchData& match_data) { return true; }
EOF

# Draft
cat > draft/draft.cpp << 'EOF'
#include "draft.h"
#include <algorithm>
#include <string_view>
#include <vector>
#include "../core/rng.h"
void play_draft() {
    const auto chosen = cambc::draft::current_draft(cambc::team::ally);
    std::vector<cambc::champion_draft> available;
    for (int i = 0; i < 11; ++i) {
        auto pick = static_cast<cambc::champion_draft>(i);
        if (std::find(chosen.begin(), chosen.end(), pick) == chosen.end()) available.push_back(pick);
    }
    cambc::draft_memory = std::string_view("", 0);
    cambc::draft::pick(available.at(random_number(available.size())));
}
EOF

# Skills (stubs)
for skill in melee_attack ranged_attack dash attack_reset single_target_damage aoe_damage stun shield heal ward consumables; do
  cat > skills/${skill}.cpp << EOF
#include "skill.h"
#include "skill_instances.h"
class ${skill^} : public Skill {
public:
    bool can_use(const TurnContext& ctx) override { return false; }
    void use(TurnContext& ctx) override {}
};
${skill^} ${skill}_instance;
EOF
done

cat > skills/skill_instances.h << 'EOF'
#pragma once
#include "skill.h"
class MeleeAttack; class RangedAttack; class Dash; class AttackReset;
class SingleTargetDamage; class AoeDamage; class Stun; class Shield;
class Heal; class Ward; class Consumables;
extern MeleeAttack melee_attack_instance; extern RangedAttack ranged_attack_instance;
extern Dash dash_instance; extern AttackReset attack_reset_instance;
extern SingleTargetDamage single_target_damage_instance; extern AoeDamage aoe_damage_instance;
extern Stun stun_instance; extern Shield shield_instance; extern Heal heal_instance;
extern Ward ward_instance; extern Consumables consumables_instance;
EOF

# Champions (stubs with skill lists)
for champ in ryth stellar tavi willow cassi issac prism vesper_vale vendetta marcella ren varg; do
  cat > champions/${champ}.cpp << EOF
#include "champion_profile.h"
#include "../skills/skill_instances.h"
ChampionProfile ${champ}_profile{.is_ranged = false, .skills = {}, .linked_pair = false};
EOF
done

cat > champions/registry.cpp << 'EOF'
#include "champion_profile.h"
#include <cambc.h>
#include <unordered_map>
extern ChampionProfile ryth_profile; extern ChampionProfile stellar_profile;
extern ChampionProfile tavi_profile; extern ChampionProfile willow_profile;
extern ChampionProfile cassi_profile; extern ChampionProfile issac_profile;
extern ChampionProfile prism_profile; extern ChampionProfile vesper_vale_profile;
extern ChampionProfile vendetta_profile; extern ChampionProfile marcella_profile;
extern ChampionProfile ren_profile; extern ChampionProfile varg_profile;
static const std::unordered_map<cambc::champion_type, ChampionProfile*> champion_registry = {
    {cambc::champion_type::ryth, &ryth_profile}, {cambc::champion_type::stellar, &stellar_profile},
    {cambc::champion_type::tavi, &tavi_profile}, {cambc::champion_type::willow, &willow_profile},
    {cambc::champion_type::cassi, &cassi_profile}, {cambc::champion_type::issac, &issac_profile},
    {cambc::champion_type::prism, &prism_profile}, {cambc::champion_type::vesper_vale, &vesper_vale_profile},
    {cambc::champion_type::vendetta, &vendetta_profile}, {cambc::champion_type::marcella, &marcella_profile},
    {cambc::champion_type::ren, &ren_profile}, {cambc::champion_type::varg, &varg_profile},
};
const ChampionProfile* get_champion_profile(cambc::champion_type type) {
    auto it = champion_registry.find(type);
    return it != champion_registry.end() ? it->second : nullptr;
}
EOF

# Bot
cat > bot.h << 'EOF'
#pragma once
#include <cambc.h>
#include "core/match_data.h"
#include "core/bot_memory.h"
#include "champions/champion_profile.h"
class Bot {
public:
    Bot();
    void play_turn();
private:
    MatchData match_data_;
    BotMemory memory_;
    const ChampionProfile* profile_;
    bool turn_guard();
    void post_turn();
};
EOF

cat > bot.cpp << 'EOF'
#include "bot.h"
#include "init/initializer.h"
#include "core/context.h"
#include "brain/state_selector.h"
#include "states/shop.cpp"
#include "states/recall.cpp"
#include "states/attack.cpp"
#include "states/tribute.cpp"
#include "states/farm.cpp"
#include "brain/state.h"
#include "constants/brain_constants.h"
Bot::Bot() : profile_(nullptr) {}
bool Bot::turn_guard() {
    auto& me = cambc::self();
    if (!me.is_alive()) {
        shop_pre_step();
        memory_.current_state = std::monostate{};
        return false;
    }
    if (me.is_recalling() || me.is_submitting_gems() || me.is_teleporting()) return false;
    if (!me.can_move() && !me.can_attack()) return false;
    return true;
}
void Bot::post_turn() {}
void Bot::play_turn() {
    if (!match_data_.initialized) run_init_stages(match_data_, memory_);
    if (!turn_guard()) return;
    shop_pre_step();
    static ChampionProfile placeholder_profile{.is_ranged = false, .skills = {}, .linked_pair = false};
    TurnContext ctx(placeholder_profile);
    StateSelector selector;
    selector.add_state(&recall_state_instance, State::Recall);
    selector.add_state(&attack_state_instance, State::Attack);
    selector.add_state(&tribute_state_instance, State::Tribute);
    selector.add_state(&farm_state_instance, State::Farm);
    selector.select_and_execute(ctx, memory_);
    post_turn();
}
EOF

# Main
cat > main.cpp << 'EOF'
#include <cambc.h>
#include <iostream>
#include "bot.h"
#include "draft/draft.h"
int main() {
    Bot bot;
    while (true) {
        try {
            if (cambc::is_draft()) play_draft();
            else bot.play_turn();
        } catch (const std::exception& error) {
            std::cout << "bot error: " << error.what() << std::endl;
        } catch (...) {
            std::cout << "bot error: unknown exception" << std::endl;
        }
        cambc::end_turn();
    }
}
EOF

echo "Implementation files created"
