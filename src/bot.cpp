#include "bot.h"
#include "init/initializer.h"
#include "core/context.h"
#include "brain/state_selector.h"
#include "states/shop.h"
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
