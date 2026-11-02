#include "state_selector.h"
#include "combat.h"
#include "farm.h"
#include "tribute.h"
#include "../core/match_data.h"
#include "../core/team_strategy.h"
#include "../nav/pathfinding.h"
#include <cambc.h>
#include <limits>

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

void reset_prism_farm_recovery(BotMemory& memory) {
    memory.prism_farm_target.reset();
    memory.prism_stall_rounds = 0;
    memory.prism_stall_last_round = std::numeric_limits<std::uint32_t>::max();
    memory.prism_stall_sample_position.reset();
    memory.prism_stall_sample_round = std::numeric_limits<std::uint32_t>::max();
    memory.prism_stall_sample_had_move_attempt = false;
    memory.prism_tether_released = false;
    memory.prism_release_move_rounds = 0;
    memory.prism_release_last_move_round = std::numeric_limits<std::uint32_t>::max();
}

void update_prism_farm_recovery(BotMemory& memory, cambc::vec2 target,
                                const FarmMovementStatus& movement, std::uint32_t round) {
    if (!team_strategy::valid_position(target)) {
        reset_prism_farm_recovery(memory);
        return;
    }
    if (!memory.prism_farm_target || *memory.prism_farm_target != target) {
        reset_prism_farm_recovery(memory);
        memory.prism_farm_target = target;
        memory.prism_stall_sample_position = cambc::self().position();
        memory.prism_stall_sample_round = round;
    }

    if (memory.prism_tether_released) {
        if (movement.attempted && movement.succeeded) {
            memory.prism_release_move_rounds =
                memory.prism_release_last_move_round != std::numeric_limits<std::uint32_t>::max() &&
                memory.prism_release_last_move_round + 1 == round
                    ? memory.prism_release_move_rounds + 1 : 1;
            memory.prism_release_last_move_round = round;
            if (memory.prism_release_move_rounds >= 3) {
                memory.prism_tether_released = false;
                memory.prism_stall_rounds = 0;
                memory.prism_stall_last_round = std::numeric_limits<std::uint32_t>::max();
                memory.prism_stall_sample_position = cambc::self().position();
                memory.prism_stall_sample_round = round;
                memory.prism_stall_sample_had_move_attempt = false;
                memory.prism_release_move_rounds = 0;
                memory.prism_release_last_move_round = std::numeric_limits<std::uint32_t>::max();
            }
        } else {
            memory.prism_release_move_rounds = 0;
            memory.prism_release_last_move_round = std::numeric_limits<std::uint32_t>::max();
        }
        return;
    }

    if (movement.attempted) memory.prism_stall_sample_had_move_attempt = true;
    constexpr std::uint32_t kStallSampleInterval = 5;
    if (memory.prism_stall_sample_position &&
        round - memory.prism_stall_sample_round >= kStallSampleInterval) {
        const auto position = cambc::self().position();
        const bool stayed_near_anchor = position.chebyshev_distance(
            *memory.prism_stall_sample_position) <= 1;
        if (memory.prism_stall_sample_had_move_attempt && stayed_near_anchor) {
            memory.prism_tether_released = true;
            memory.prism_stall_rounds = 0;
            memory.prism_stall_last_round = std::numeric_limits<std::uint32_t>::max();
            memory.prism_release_move_rounds = 0;
            memory.prism_release_last_move_round = std::numeric_limits<std::uint32_t>::max();
            return;
        }

        memory.prism_stall_sample_position = position;
        memory.prism_stall_sample_round = round;
        memory.prism_stall_sample_had_move_attempt = false;
    }

    if (movement.attempted && !movement.succeeded) {
        memory.prism_stall_rounds =
            memory.prism_stall_last_round != std::numeric_limits<std::uint32_t>::max() &&
            memory.prism_stall_last_round + 1 == round
                ? memory.prism_stall_rounds + 1 : 1;
        memory.prism_stall_last_round = round;
        if (memory.prism_stall_rounds >= 3) {
            memory.prism_tether_released = true;
            memory.prism_release_move_rounds = 0;
            memory.prism_release_last_move_round = std::numeric_limits<std::uint32_t>::max();
        }
    } else {
        memory.prism_stall_rounds = 0;
        memory.prism_stall_last_round = std::numeric_limits<std::uint32_t>::max();
    }
}

bool final_tribute_turn_flow(const ChampionProfile& profile, MatchData& match, BotMemory& memory,
                             bool prism) {
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
        if (prism) publish_prism_recall();
        pause_final_tribute_for_recall(memory);
    }
    if (recall_handled) {
        if (prism && force_recall && !memory.recall_channel_started &&
            !cambc::map::spawn_region(cambc::team::ally).is_set(cambc::self().position()))
            publish_prism_recall();
        return true;
    }
    if (final_tribute_turn(memory)) {
        if (prism) team_strategy::publish(team_strategy::Mode::farming, cambc::self().position());
        return true;
    }
    (void)profile;
    return false;
}

bool handle_prism(const ChampionProfile& profile, MatchData& match, BotMemory& memory) {
    const auto command = team_strategy::read();
    if (command.mode != team_strategy::Mode::farming &&
        command.mode != team_strategy::Mode::farming_unleashed)
        reset_prism_farm_recovery(memory);
    const auto required_acks = team_strategy::required_recall_ack_mask();
    if (!final_tribute_active() && command.mode == team_strategy::Mode::arrived &&
        (team_strategy::recall_ack_mask() & required_acks) != required_acks) {
        // Keep Prism at spawn until every teammate has observed the arrival signal.
        return true;
    }

    constexpr float kPrismVisionDistance = 8.0f;
    if (recall_turn(match, memory, false, kPrismVisionDistance)) {
        reset_prism_farm_recovery(memory);
        publish_prism_recall();
        return true;
    }

    cambc::vec2 enemy_position{127, 127};
    if (combat_turn(match, memory, 100.0f, &enemy_position, true)) {
        reset_prism_farm_recovery(memory);
        publish_combat(enemy_position);
        return true;
    }

    if (prism_is_alone(memory) &&
        recall_turn(match, memory, true, kPrismVisionDistance)) {
        reset_prism_farm_recovery(memory);
        publish_prism_recall();
        return true;
    }

    cambc::vec2 camp_target{127, 127};
    FarmMovementStatus movement;
    farm_turn(profile, match, memory, std::nullopt, &camp_target, &movement);
    const auto round = cambc::round_number();
    update_prism_farm_recovery(memory, camp_target, movement, round);
    const auto mode = memory.prism_tether_released
        ? team_strategy::Mode::farming_unleashed : team_strategy::Mode::farming;
    team_strategy::publish(mode, cambc::self().position(), camp_target);
    return true;
}

bool handle_teammate(const ChampionProfile& profile, MatchData& match, BotMemory& memory) {
    // A teammate's own low-health or shop recall may leave the Prism tether.
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
            nav::move_into_radius(memory.movement, command.prism_position, 2);
            return true;
        case team_strategy::Mode::arrived:
            if (cambc::self().health() >= cambc::self().max_health() &&
                team_strategy::valid_position(command.prism_position) &&
                cambc::self().position().chebyshev_distance(command.prism_position) <= 2)
                return true;
            if (recall_turn(match, memory, true)) return true;
            return true;
        case team_strategy::Mode::farming:
        case team_strategy::Mode::farming_unleashed:
            if (!team_strategy::valid_position(command.target_position)) return true;
            if (command.mode == team_strategy::Mode::farming) {
                const auto position = cambc::self().position();
                const auto distance = position.chebyshev_distance(command.prism_position);
                if (distance > 3) {
                    nav::move_into_radius(memory.movement, command.prism_position, 3, true);
                    return true;
                }
                memory.movement.movement_guard_center = command.prism_position;
                memory.movement.movement_guard_radius = 3;
            }
            farm_turn(profile, match, memory, command.target_position);
            memory.movement.movement_guard_center.reset();
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
    if (prism && in_final_tribute) reset_prism_farm_recovery(memory);

    if (in_final_tribute) {
        if (final_tribute_turn_flow(profile, match, memory, prism)) return;
    }

    if (prism) handle_prism(profile, match, memory);
    else handle_teammate(profile, match, memory);
}
