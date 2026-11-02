#include "bot.h"
#include "init/initializer.h"
#include "states/state_selector.h"
#include "states/shop.h"
#include "states/tribute.h"
#include "core/team_strategy.h"
#include "champions/registry.h"
#include <cambc.h>

namespace {
constexpr std::uint32_t kPrismRespawnRounds = 60;
constexpr std::uint32_t kPrismRespawnRegroupLead = 5;

bool prism_has_home_emblem() {
    try {
        const auto counts = cambc::self().item_count();
        return counts[cambc::item_type::home_emblem_of_strength] > 0 ||
            counts[cambc::item_type::home_emblem_of_focus] > 0 ||
            counts[cambc::item_type::home_emblem_of_health] > 0;
    } catch (const cambc::error&) {
        return false;
    }
}
}

Bot::Bot() : profile_(nullptr) {}

bool Bot::turn_guard() {
    auto& me = cambc::self();
    if (!me.is_alive()) {
        return false;
    }
    if (me.is_teleporting()) return false;
    if (me.is_recalling()) return false;
    const bool submitting_gems = me.is_submitting_gems();
    if (!me.can_move() && !me.can_attack() && !me.can_cast() && !submitting_gems) return false;
    return true;
}

void Bot::play_turn() {
    if (!match_data_.initialized) {
        run_init_stages(match_data_);
    }
    if (!match_data_.initialized) {
        return;
    }

    auto& me = cambc::self();
    const auto round = cambc::round_number();
    if (!memory_.movement.initialized) {
        const auto champion = me.champion_name();
        memory_.tribute_eligible = champion != cambc::champion_type::prism;
        memory_.movement.is_heart_of_ruin = champion == cambc::champion_type::heart_of_ruin;
        memory_.movement.resets_cooldowns_on_kill = champion == cambc::champion_type::vendetta;
        profile_ = get_champion_profile(champion);
        const auto abilities = me.abilities();
        auto& dash = abilities[nav::kDashSlot];
        if (dash.is_dash()) {
            memory_.movement.has_dash = true;
            memory_.movement.ability = dash;
            memory_.movement.cooldown = dash.cooldown();
        }
        memory_.movement.initialized = true;
    } else if (memory_.movement.cooldown > 0) {
        --memory_.movement.cooldown;
    }

    update_final_tribute(memory_);

    const bool is_prism = me.champion_name() == cambc::champion_type::prism;
    const auto command = team_strategy::read();
    if (is_prism && !me.is_alive()) {
        team_strategy::publish(team_strategy::Mode::prism_dead, {127, 127});
    } else if (!is_prism && command.mode == team_strategy::Mode::prism_dead) {
        if (memory_.prism_dead_since_round == std::numeric_limits<std::uint32_t>::max())
            memory_.prism_dead_since_round = round;
        if (round >= memory_.prism_dead_since_round &&
            round - memory_.prism_dead_since_round >= kPrismRespawnRounds - kPrismRespawnRegroupLead)
            memory_.prism_regroup_active = true;
    } else if (is_prism) {
        if (command.mode != team_strategy::Mode::arrived) {
            const auto target = team_strategy::valid_position(command.target_position)
                ? std::optional<cambc::vec2>{command.target_position} : std::nullopt;
            team_strategy::publish(command.mode, me.position(), target);
        }
    } else {
        memory_.prism_dead_since_round = std::numeric_limits<std::uint32_t>::max();
        memory_.prism_regroup_active = false;
    }

    if (!is_prism && !final_tribute_active() && command.mode == team_strategy::Mode::arrived &&
        me.is_alive()) {
        const bool on_spawn = cambc::map::spawn_region(cambc::team::ally).is_set(me.position());
        const bool healthy_near_prism = team_strategy::valid_position(command.prism_position) &&
            me.health() >= me.max_health() &&
            me.position().chebyshev_distance(command.prism_position) <= 2;
        if (on_spawn || healthy_near_prism) {
            if (const auto slot = team_strategy::self_slot()) team_strategy::acknowledge_recall_arrival(*slot);
        }
    }

    if (memory_.recall_channel_started) {
        if (me.is_recalling()) {
            if (is_prism) {
                const auto mode = prism_has_home_emblem() ? team_strategy::Mode::recalling_home
                                                           : team_strategy::Mode::recalling;
                team_strategy::publish(mode, me.position());
            }
            return;
        }
        memory_.recall_channel_started = false;
        if (cambc::map::spawn_region(cambc::team::ally).is_set(me.position())) {
            memory_.spawn_arrival_round = round;
            if (is_prism) {
                if (!final_tribute_active()) team_strategy::clear_recall_ack_mask();
                team_strategy::publish(team_strategy::Mode::arrived, me.position());
            }
        }
    }
    shop_at_spawn_or_death();
    if (memory_.spawn_arrival_round == round) return;

    if (!turn_guard()) return;
    if (!profile_) return;

    run_state_selector(*profile_, match_data_, memory_);
}
