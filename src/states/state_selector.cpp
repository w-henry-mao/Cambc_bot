#include "state_selector.h"
#include "combat.h"
#include "farm.h"
#include "tribute.h"
#include "../core/match_data.h"
#include "../core/team_strategy.h"
#include "../nav/pathfinding.h"
#include <cambc.h>

namespace {

bool is_prism() {
    return cambc::self().champion_name() == cambc::champion_type::prism;
}

bool has_home_emblem() {
    try {
        const auto counts = cambc::self().item_count();
        return counts[cambc::item_type::home_emblem_of_strength] > 0 ||
            counts[cambc::item_type::home_emblem_of_focus] > 0 ||
            counts[cambc::item_type::home_emblem_of_health] > 0;
    } catch (const cambc::error&) {
        return false;
    }
}

void publish_prism_recall() {
    const auto mode = has_home_emblem() ? team_strategy::Mode::recalling_home
                                        : team_strategy::Mode::recalling;
    team_strategy::publish(mode, cambc::self().position());
}

bool prism_is_alone(BotMemory& memory) {
    const auto round = cambc::round_number();
    const auto self = cambc::self();
    const auto position = self.position();
    bool ally_near = false;
    for (const auto& ally : cambc::vision::champions(cambc::team::ally)) {
        try {
            if (static_cast<cambc::handle_champion_t>(ally) == static_cast<cambc::handle_champion_t>(self) ||
                !ally.is_alive()) continue;
            if (ally.position().chebyshev_distance(position) <= 3) {
                ally_near = true;
                break;
            }
        } catch (const cambc::error&) { }
    }

    if (ally_near) {
        memory.prism_alone_rounds = 0;
        memory.prism_alone_last_round = round;
    } else if (memory.prism_alone_last_round != round) {
        memory.prism_alone_rounds = memory.prism_alone_last_round + 1 == round
            ? memory.prism_alone_rounds + 1 : 1;
        memory.prism_alone_last_round = round;
    }
    return memory.prism_alone_rounds > 3;
}

void publish_combat(cambc::vec2 target) {
    team_strategy::publish(team_strategy::Mode::combat, cambc::self().position(), target);
}

bool final_tribute_turn_flow(MatchData& match, BotMemory& memory, bool prism) {
    cambc::vec2 target{127, 127};
    if (combat_turn(match, memory, prism ? 100.0f : 5.0f, prism ? &target : nullptr, prism)) {
        if (prism) publish_combat(target);
        return true;
    }

    const bool force_recall = final_tribute_recall_phase();
    bool recall_handled = false;
    if (prism) {
        recall_handled = recall_turn(match, memory, force_recall, 100.0f);
    } else {
        recall_handled = recall_turn(match, memory, false, 5.0f);
        if (!recall_handled && !force_recall) {
            const auto command = team_strategy::read();
            if (command.mode == team_strategy::Mode::combat &&
                team_strategy::valid_position(command.target_position)) {
                nav::move_towards(memory.movement, command.target_position, true, false);
                return true;
            }
        }
        if (!recall_handled && force_recall)
            recall_handled = recall_turn(match, memory, true, 5.0f);
    }
    if (memory.recall_channel_started) {
        if (prism) {
            publish_prism_recall();
        }
        pause_final_tribute_for_recall(memory);
    }
    if (recall_handled) {
        if (prism && force_recall && !memory.recall_channel_started &&
            !cambc::map::spawn_region(cambc::team::ally).is_set(cambc::self().position()))
            publish_prism_recall();
        return true;
    }
    cambc::vec2 tribute_target{127, 127};
    if (final_tribute_turn(memory, prism ? &tribute_target : nullptr)) {
        if (prism)
            team_strategy::publish(team_strategy::Mode::farming,
                                   cambc::self().position(), tribute_target);
        return true;
    }
    return false;
}

bool handle_prism(const ChampionProfile& profile, MatchData& match, BotMemory& memory) {
    const auto command = team_strategy::read();
    const auto required_acks = team_strategy::required_recall_ack_mask();
    if (!final_tribute_active() && command.mode == team_strategy::Mode::arrived &&
        (team_strategy::recall_ack_mask() & required_acks) != required_acks) {
        // Keep Prism at spawn until every teammate has observed the arrival signal.
        return true;
    }

    constexpr float kPrismVisionDistance = 8.0f;
    if (recall_turn(match, memory, false, kPrismVisionDistance)) {
        publish_prism_recall();
        return true;
    }

    cambc::vec2 enemy_position{127, 127};
    if (combat_turn(match, memory, 100.0f, &enemy_position, true)) {
        publish_combat(enemy_position);
        return true;
    }

    if (prism_is_alone(memory) &&
        recall_turn(match, memory, true, kPrismVisionDistance)) {
        publish_prism_recall();
        return true;
    }

    cambc::vec2 camp_target{127, 127};
    farm_turn(profile, match, memory, std::nullopt, &camp_target);
    team_strategy::publish(team_strategy::Mode::farming, cambc::self().position(), camp_target);
    return true;
}

bool handle_teammate(const ChampionProfile& profile, MatchData& match, BotMemory& memory) {
    // A teammate's own low-health or shop recall may temporarily move it away from Prism.
    if (recall_turn(match, memory)) return true;
    if (combat_turn(match, memory)) return true;

    const auto command = team_strategy::read();
    switch (command.mode) {
        case team_strategy::Mode::combat:
            if (team_strategy::valid_position(command.target_position))
                nav::move_towards(memory.movement, command.target_position, true, false);
            return true;
        case team_strategy::Mode::recalling:
        case team_strategy::Mode::recalling_home:
            recall_turn(match, memory, true);
            return true;
        case team_strategy::Mode::arrived:
            if (cambc::self().health() >= cambc::self().max_health() &&
                team_strategy::valid_position(command.prism_position) &&
                cambc::self().position().chebyshev_distance(command.prism_position) <= 2)
                return true;
            if (recall_turn(match, memory, true)) return true;
            return true;
        case team_strategy::Mode::farming:
        case team_strategy::Mode::legacy_tether_released:
            if (!team_strategy::valid_position(command.target_position)) return true;
            if (!final_tribute_active() && handle_cleared_coordinated_target(
                    match, memory, command.target_position, command.prism_position)) return true;
            memory.movement.movement_guard_center.reset();
            farm_turn(profile, match, memory, command.target_position);
            return true;
        case team_strategy::Mode::independent:
            farm_turn(profile, match, memory);
            return true;
        case team_strategy::Mode::prism_dead:
            if (memory.prism_regroup_active) {
                // Keep the regroup latch set while combat takes priority; otherwise
                // recall and wait at spawn until Prism returns.
                recall_turn(match, memory, true);
                return true;
            }
            if (tribute_turn(memory)) return true;
            farm_turn(profile, match, memory);
            return true;
    }
    farm_turn(profile, match, memory);
    return true;
}

}

void run_state_selector(const ChampionProfile& profile, MatchData& match, BotMemory& memory) {
    memory.movement.movement_guard_center.reset();
    update_tribute_progress(memory);
    const bool prism = is_prism();
    const bool in_final_tribute = final_tribute_active();

    if (in_final_tribute) {
        if (final_tribute_turn_flow(match, memory, prism)) return;
    }

    if (prism) handle_prism(profile, match, memory);
    else handle_teammate(profile, match, memory);
}
