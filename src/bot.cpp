#include "bot.h"
#include "init/initializer.h"
#include "states/farm.h"
#include "states/shop.h"
#include "champions/registry.h"
#include <cambc.h>

Bot::Bot() : profile_(nullptr) {}

bool Bot::turn_guard() {
    auto& me = cambc::self();
    if (!me.is_alive()) {
        return false;
    }
    if (me.is_teleporting()) return false;
    if (me.is_recalling() || me.is_submitting_gems()) {
        return false;
    }
    if (!me.can_move() && !me.can_attack() && !me.can_cast()) return false;
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

    if (memory_.recall_channel_started) {
        if (me.is_recalling()) return;
        memory_.recall_channel_started = false;
        if (cambc::map::spawn_region(cambc::team::ally).is_set(me.position())) {
            memory_.spawn_arrival_round = round;
        }
    }
    shop_at_spawn_or_death();
    if (memory_.spawn_arrival_round == round) return;

    if (!turn_guard()) {
        return;
    }

    if (!profile_) {
        return;
    }
    farm_turn(*profile_, match_data_, memory_);
}
