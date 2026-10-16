#include "bot.h"
#include "debug/fuel_meter.h"
#include "init/initializer.h"
#include "core/context.h"
#include "core/world.h"
#include "brain/state_selector.h"
#include "brain/channel_safety.h"
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
    if (me.is_teleporting()) return false;   // cannot be cancelled
    if (me.is_recalling() || me.is_submitting_gems()) {
        if (channel_still_safe(match_data_)) return false;
        // One cancel per turn; once cancelled the champion acts normally this turn.
        try {
            if (me.is_recalling()) me.cancel_recall();
            else me.cancel_submit_gems();
        } catch (const std::exception&) {
            return false;
        }
    }
    if (!me.can_move() && !me.can_attack()) return false;
    return true;
}
void Bot::post_turn() {}
void Bot::play_turn() {
    debug::FuelMeter whole("turn total");
    if (!match_data_.initialized) run_init_stages(match_data_, memory_);
    if (!match_data_.initialized) return;
    if (!turn_guard()) {
        if (cambc::self().is_alive()) match_data_.last_health = cambc::self().health();
        return;
    }
    update_world(match_data_);
    shop_pre_step();
    static ChampionProfile placeholder_profile{.is_ranged = false, .skills = {}, .linked_pair = false};
    TurnContext ctx(placeholder_profile);
    ctx.match_data = &match_data_;
    StateSelector selector;
    selector.add_state(&recall_state_instance, State::Recall);
    selector.add_state(&attack_state_instance, State::Attack);
    selector.add_state(&tribute_state_instance, State::Tribute);
    selector.add_state(&farm_state_instance, State::Farm);
    selector.select_and_execute(ctx, memory_);
    post_turn();
    debug::report_fuel();
}
