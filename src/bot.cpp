#include "bot.h"
#include "debug/fuel_meter.h"
#include "init/initializer.h"
#include "core/context.h"
#include "core/world.h"
#include "brain/state_selector.h"
#include "brain/team_fight_call.h"
#include "comms/ally_positions.h"
#include "constants/combat_constants.h"
#include "combat/sdk_trace.h"
#include "combat/sdk_duel_mode.h"
#include "combat/sdk_team_fight.h"
#include "states/shop.h"
#include "states/recall.h"
#include "states/attack.h"
#include "states/tribute.h"
#include "states/farm.h"
#include "brain/state.h"
#include "constants/brain_constants.h"
Bot::Bot() : profile_(nullptr) {}
bool Bot::turn_guard() {
    auto& me = cambc::self();
    if (!me.is_alive()) {
        shop_pre_step();
        match_data_.last_health = -1;
        return false;
    }
    if (me.is_recalling() || me.is_submitting_gems() || me.is_teleporting()) return false;
    if (!me.can_move() && !me.can_attack()) return false;
    return true;
}
void Bot::post_turn() {
    combat_trace_turn();
    team_fight_trace_turn();   // test builds only
    duel_mode_end_of_turn();   // test builds only
}
void Bot::play_turn() {
    debug::FuelMeter whole("turn total");
    struct PostTurn { Bot* bot; ~PostTurn() { bot->post_turn(); } } post_turn_guard{this};   // runs on every exit
    if (!match_data_.initialized) run_init_stages(match_data_, memory_);
    if (!match_data_.initialized) return;
    if (cambc::self().is_alive()) {   // also while recalling or channelling
        share_ally_positions();
        if constexpr (constants::kTeamFightSharedAllies) remember_teammates();
    }
    if (!turn_guard()) {
        if (cambc::self().is_alive()) match_data_.last_health = cambc::self().health();
        return;
    }
    update_world(match_data_);
    shop_pre_step();
    if (duel_mode_turn()) return;   // test builds only
    static ChampionProfile placeholder_profile{.is_ranged = false, .skills = {}, .linked_pair = false};
    TurnContext ctx(placeholder_profile);
    ctx.match_data = &match_data_;
    StateSelector selector;
    selector.add_state(&recall_state_instance, State::Recall);
    selector.add_state(&attack_state_instance, State::Attack);
    selector.add_state(&tribute_state_instance, State::Tribute);
    selector.add_state(&farm_state_instance, State::Farm);
    decide_team_fight_call();
    combat_trace_turn_start();
    selector.select_and_execute(ctx, memory_);
    debug::report_fuel();
}
