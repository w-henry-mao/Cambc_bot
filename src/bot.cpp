#include <iostream>
#include "bot.h"
#include "core/roster.h"
#include "debug/fuel_meter.h"
#include "init/initializer.h"
#include "init/deferred_setup.h"
#include "core/context.h"
#include "core/world.h"
#include "brain/state_selector.h"
#include "brain/team_fight_call.h"
#include "comms/ally_positions.h"
#include "comms/team_comms.h"
#include "debug/test_altar_locker.h"
#include "skills/altar_locks.h"
#include "constants/test_hooks.h"
#include "nav/safe_recall.h"
#include "constants/combat_constants.h"
#include "combat/sdk_trace.h"
#include "combat/sdk_duel_mode.h"
#include "combat/sdk_team_fight.h"
#include "brain/channel_safety.h"
#include "states/shop.h"
#include "states/recall.h"
#include "states/attack.h"
#include "states/tribute.h"
#include "states/farm.h"
#include "brain/state.h"
#include "constants/brain_constants.h"
#include "constants/state_constants.h"
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
void Bot::post_turn() {
    if constexpr (SCORE_TRACE) {   // test builds: banked gems every 100 rounds, logged by the first ally
        const auto& allies = roster_allies();
        if (cambc::round_number() % 100 == 0 && !allies.empty() && allies.front() == cambc::self())
            std::cout << "SCORE r=" << cambc::round_number() << " ours=" << cambc::global_submitted_gems(cambc::team::ally)
                      << " theirs=" << cambc::global_submitted_gems(cambc::team::enemy) << '\n';
    }
    combat_trace_turn();
    team_fight_trace_turn();   // test builds only
    recall_trace_turn();       // test builds only
    duel_mode_end_of_turn();   // test builds only
    continue_deferred_setup(match_data_);   // last: uses whatever fuel the turn left
}
void Bot::play_turn() {
    debug::FuelMeter whole("turn total");
    struct PostTurn { Bot* bot; ~PostTurn() { bot->post_turn(); } } post_turn_guard{this};   // runs on every exit
    if (!match_data_.initialized) run_init_stages(match_data_, memory_);
    if (!match_data_.initialized) return;
    sweep_team_header();   // every champion, every turn, dead or alive (comms/shared_memory.h)
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
    test_place_altar_lock();   // test opponent builds only
    place_altar_lock();        // constants::kAltarLockPlan (0: never)
    if constexpr (constants::kBunchMode == 1) {   // rally test: broadcast where enemies are near us
        const auto here = cambc::self().position();
        for (const auto& e : roster_enemies())
            if (in_view(e) && e.position().chebyshev_distance(here) <= static_cast<std::uint32_t>(constants::kRallyTrigger)) {
                write_rally(here.x, here.y);
                break;
            }
    }
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
    if constexpr (MAP_DUMP) {
        // Once: map size, then each camp group with its walk from our and the enemy's spawn.
        static bool dumped = false;
        if (!dumped && match_data_.initialized && !roster_allies().empty() && roster_allies()[0] == cambc::self()) {
            dumped = true;
            const auto blockers = cambc::map::walls() | cambc::map::holes();
            cambc::distance_field ours, theirs;
            cambc::bitboard visited;
            ours.compute(blockers, cambc::map::spawn_region(cambc::team::ally), visited);
            theirs.compute(blockers, cambc::map::spawn_region(cambc::team::enemy), visited);
            std::cout << "MAP " << cambc::map::width() << ' ' << cambc::map::height() << " altars " << cambc::map::altars().count()
                      << " bushes " << cambc::map::bushes().count() << " walls " << cambc::map::walls().count() << '\n';
            for (const auto& g : match_data_.camp_groups)
                std::cout << "GROUP " << g.centre.x << ' ' << g.centre.y << " gems " << g.gem_value << " gold " << g.gold_value
                          << " hp " << g.design_health << " str " << g.design_strength << " camps " << g.spawn_positions.size()
                          << " ours " << ours.get(g.centre) << " theirs " << theirs.get(g.centre) << '\n';
            for (const auto& g : cambc::map::camp_groups())
                for (const auto& c : g) std::cout << "CAMP " << c.spawn_position().x << ' ' << c.spawn_position().y << " hp " << c.base_stats().base_health << '\n';
            for (const auto a : cambc::map::altars().positions())
                std::cout << "ALTAR " << a.x << ' ' << a.y << " ours " << ours.get(a) << " theirs " << theirs.get(a) << '\n';
        }
    }
    if constexpr (constants::kAltarMemory) note_altar_locks(match_data_);
    selector.select_and_execute(ctx, memory_);
    if constexpr (STATE_TRACE) {
        const auto& me = cambc::self();
        std::cout << "ST " << cambc::round_number() << ' ' << cambc::to_string(me.champion_name()) << ' '
                  << static_cast<int>(last_selected_state());
        if (me.is_alive()) {
            // ... visible enemies, nearest enemy distance (-1: none), farm target group (-1: none).
            int seen = 0, nearest = -1;
            for (const auto& e : roster_enemies())
                if (in_view(e)) {
                    ++seen;
                    const int d = static_cast<int>(e.position().chebyshev_distance(me.position()));
                    if (nearest < 0 || d < nearest) nearest = d;
                }
            std::cout << ' ' << me.position().x << ' ' << me.position().y << ' ' << me.gem_count() << ' '
                      << me.health() << ' ' << me.max_health() << ' ' << seen << ' ' << nearest << ' '
                      << match_data_.farm_target.value_or(-1);
        }
        std::cout << '\n';
    }
    debug::report_fuel();
}
