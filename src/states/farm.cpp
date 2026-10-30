#include "farm.h"
#include "../core/match_data.h"
#include "../champions/champion_profile.h"
#include "../nav/pathfinding.h"
#include "../init/deferred_setup.h"
#include "../constants/farm_constants.h"
#include <cambc.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

namespace {
constexpr int kUnreached = 65535;
constexpr int kDashCooldown = 8;
constexpr std::uint32_t kRewardBeliefRounds = 20;
constexpr std::uint32_t kYieldCaptureRounds = 5;
constexpr int kHarvestNearRadius = 5;

int champion_rank(cambc::champion_type type) {
    switch (type) {
        case cambc::champion_type::prism: return 1;
        case cambc::champion_type::vesper_vale: return 2;
        case cambc::champion_type::marcella: return 3;
        case cambc::champion_type::vendetta: return 4;
        case cambc::champion_type::heart_of_ruin: return 5;
        default: return 99;
    }
}

struct VisibleAlly { int rank; cambc::vec2 position; };

struct CampThreat {
    cambc::vec2 position;
    std::int32_t attack_range;
    bool damaging;
};

void observe_respawns(MatchData& match, std::uint32_t now) {
    if (match.respawn_observed_round == now) return;
    cambc::bitboard visible_tiles;
    cambc::bitboard live_camp_tiles;
    cambc::vision::visible_tiles_into(visible_tiles);
    cambc::vision::camp_tiles_into(live_camp_tiles);
    for (auto& group : match.camp_groups) {
        ensure_spawn_data(group);
        if ((group.spawn_tiles & visible_tiles).is_empty()) {
            continue;
        }

        if (!(group.spawn_tiles & live_camp_tiles).is_empty()) {
            // Living camps are already known; skip group.state().
            group.respawn_ready_round = 0;
            group.blocked_round = std::numeric_limits<std::uint32_t>::max();
            group.reward_waiting = false;
            group.yield_until_round = 0;
        } else {
            // The last observed timer gives an exact next-check round. Avoid querying a
            // sleeping group every turn; live_camp_tiles above still detects its respawn.
            if (group.respawn_ready_round > now && !(group.yield_until_round && now >= group.yield_until_round)) {
                continue;
            }
            if (group.blocked_round != std::numeric_limits<std::uint32_t>::max() &&
                now >= group.blocked_round &&
                now - group.blocked_round < farm_constants::kBlockedSkipRounds) {
                continue;
            }

            const auto state = group.view->state();
            if (std::holds_alternative<cambc::map::camp_group_alive>(state)) {
                group.respawn_ready_round = 0;
                group.blocked_round = std::numeric_limits<std::uint32_t>::max();
                if (group.yield_until_round && now >= group.yield_until_round) {
                    group.yield_until_round = 0;
                    group.reward_waiting = true;
                    group.reward_seen_round = now;
                }
            } else if (const auto* respawn = std::get_if<cambc::map::camp_group_respawning>(&state)) {
                group.reward_waiting = false;
                if (respawn->timer > 0) {
                    group.respawn_ready_round = now + static_cast<std::uint32_t>(respawn->timer);
                    group.blocked_round = std::numeric_limits<std::uint32_t>::max();
                } else {
                    group.respawn_ready_round = now;
                    group.blocked_round = now;
                }
            } else {
                group.reward_waiting = true;
                group.reward_seen_round = now;
                group.respawn_ready_round = 0;
                group.blocked_round = std::numeric_limits<std::uint32_t>::max();
            }
        }
    }
    match.respawn_observed_round = now;
}

void note_kill_cooldown_reset(BotMemory& memory, const cambc::camp& target) {
    if (!memory.movement.resets_cooldowns_on_kill) return;
    try {
        if (target.health() <= 0.0f) memory.movement.cooldown = 0;
    } catch (const cambc::error&) {
        // Keep the local cooldown when the target is no longer queryable.
    }
}

// Calculate champion's DPS
float calculate_dps(const ChampionProfile& profile) {
    auto& me = cambc::self();
    const float strength = me.strength();
    const float focus = me.focus();

    float ability_damage = 0.0f;

    // Ability 1
    if (profile.ability1_is_attack_reset) {
        // Attack reset: add strength for extra basic attack
        ability_damage += strength;
    } else if (profile.ability1_damage_multiplier > 0.0f) {
        // Damaging ability: add multiplier * focus
        ability_damage += profile.ability1_damage_multiplier * focus;
    }
    // Otherwise: neither attack reset nor damaging (e.g., shield, plain dash), add nothing

    // Ability 2
    if (profile.ability2_is_attack_reset) {
        // Attack reset: add strength for extra basic attack
        ability_damage += strength;
    } else if (profile.ability2_damage_multiplier > 0.0f) {
        // Damaging ability: add multiplier * focus
        ability_damage += profile.ability2_damage_multiplier * focus;
    }
    // Otherwise: neither attack reset nor damaging, add nothing

    return strength + ability_damage;
}

// Check if champion has Capture emblem (reduces capture time to 3 rounds)
bool has_capture_emblem() {
    try {
        auto& me = cambc::self();
        const auto counts = me.item_count();
        // Check for tribute emblem items
        if (counts[cambc::item_type::tribute_emblem_of_strength] > 0) return true;
        if (counts[cambc::item_type::tribute_emblem_of_focus] > 0) return true;
        if (counts[cambc::item_type::tribute_emblem_of_health] > 0) return true;
    } catch (const cambc::error&) {
        // If we can't read items, assume no emblem
    }
    return false;
}

// Get capture time (3 or 5 rounds)
int get_capture_time() {
    return has_capture_emblem() ? farm_constants::kCaptureTimeWithEmblem : farm_constants::kCaptureTimeNormal;
}

// Calculate discount factor
float discount(float rounds) {
    return std::pow(farm_constants::kRoundDiscount, rounds);
}

// Calculate camp value
std::optional<float> camp_value(const CachedCampGroup& group, float walk, float dps, int capture_time) {
    if (dps <= 0) return std::nullopt;

    const int kill = static_cast<int>(group.design_health / dps) + 1;
    const float reward = static_cast<float>(group.gem_value) + farm_constants::kGoldWeight * static_cast<float>(group.gold_value);

    return reward * discount(walk + static_cast<float>(kill + capture_time));
}

// Select best camp based on value
std::optional<int> select_best_camp(const MatchData& match, const cambc::distance_field& field,
                                    float dps, std::uint32_t now, float walk_factor) {
    std::optional<int> best_group;
    float best_value = 0.0f;
    const int capture_time = get_capture_time();

    for (int i = 0; i < static_cast<int>(match.camp_groups.size()); ++i) {
        const auto& group = match.camp_groups[static_cast<std::size_t>(i)];
        if (group.blocked_round != std::numeric_limits<std::uint32_t>::max() &&
            now >= group.blocked_round && now - group.blocked_round < farm_constants::kBlockedSkipRounds) {
            continue;
        }

        const int walk = field.get(group.centre);
        if (walk == kUnreached) {
            continue;
        }

        if (group.reward_waiting && now - group.reward_seen_round >= kRewardBeliefRounds) continue;
        const float adjusted_walk = static_cast<float>(walk) * walk_factor;
        const float raw_walk = static_cast<float>(walk);
        const float reward = static_cast<float>(group.gem_value) + farm_constants::kGoldWeight * static_cast<float>(group.gold_value);
        const float value = reward * discount(group.reward_waiting
            ? raw_walk + static_cast<float>(capture_time)
            : raw_walk + static_cast<float>(static_cast<int>(group.design_health / dps) + 1 + capture_time) +
                std::max(0.0f, static_cast<float>(respawn_remaining(group, now)) - 0.8f * raw_walk));
        if (!best_group || value > best_value) {
            best_group = i;
            best_value = value;
        }
    }
    return best_group;
}

// Comparator for camp candidates
bool is_better_candidate(const cambc::camp& a, const cambc::camp& b, const cambc::vec2& my_pos) {
    // Lower CAMP_PRIORITY_RANK
    int rank_a = farm_constants::get_camp_priority_rank(a.kind());
    int rank_b = farm_constants::get_camp_priority_rank(b.kind());
    if (rank_a != rank_b) return rank_a < rank_b;

    // Smaller Chebyshev distance from me
    int dist_a = static_cast<int>(a.position().chebyshev_distance(my_pos));
    int dist_b = static_cast<int>(b.position().chebyshev_distance(my_pos));
    if (dist_a != dist_b) return dist_a < dist_b;

    // Lower current health
    if (a.health() != b.health()) return a.health() < b.health();

    // Deterministic tie-break: lowest y then x
    if (a.position().y != b.position().y) return a.position().y < b.position().y;
    return a.position().x < b.position().x;
}

const cambc::camp* best_attack_target(const std::vector<const cambc::camp*>& candidates,
                                      const cambc::bitboard& attack_tiles,
                                      const cambc::vec2& my_pos) {
    const cambc::camp* best = nullptr;
    for (const auto* camp : candidates) {
        try {
            if (camp->is_visible() && camp->health() > 0 && attack_tiles.is_set(camp->position()) &&
                (!best || is_better_candidate(*camp, *best, my_pos))) {
                best = camp;
            }
        } catch (const cambc::error&) {
            continue;
        }
    }
    return best;
}

}

void farm_turn(const ChampionProfile& profile, MatchData& match, BotMemory& memory) {
    auto& me = cambc::self();
    std::optional<decltype(me.abilities())> heart_abilities;
    bool heart_shield_can_cast = false;
    if (memory.movement.is_heart_of_ruin) {
        heart_abilities.emplace(me.abilities());
        auto& shield = (*heart_abilities)[0];
        heart_shield_can_cast = shield.is_ready() && shield.is_shield() && me.can_cast();
    }

    std::optional<cambc::vec2> cached_self_position;
    const float dps = calculate_dps(profile);
    static cambc::distance_field field;
    static std::uint32_t field_round = 0;
    const auto round = cambc::round_number();
    observe_respawns(match, round);

    bool has_reward_belief = false;
    for (auto& group : match.camp_groups) {
        if (!group.max_respawn_cached && group.view) {
            group.max_respawn = group.view->max_respawn_timer();
            group.max_respawn_cached = true;
        }
        if (group.reward_waiting && round - group.reward_seen_round >= kRewardBeliefRounds) {
            group.reward_waiting = false;
            group.respawn_ready_round = group.reward_seen_round + kRewardBeliefRounds + static_cast<std::uint32_t>(group.max_respawn);
        }
        has_reward_belief = has_reward_belief || group.reward_waiting;
    }
    std::vector<VisibleAlly> visible_allies;
    if (has_reward_belief) {
        const auto my_rank = champion_rank(me.champion_name());
        for (const auto& ally : cambc::vision::champions(cambc::team::ally)) {
            try {
                if (ally.is_alive() && static_cast<cambc::handle_champion_t>(ally) != static_cast<cambc::handle_champion_t>(me))
                    visible_allies.push_back({champion_rank(ally.champion_name()), ally.position()});
            } catch (const cambc::error&) {}
        }
        for (auto& group : match.camp_groups) {
            if (!group.reward_waiting) continue;
            if (!group.harvest_near_ready) {
                for (const auto tile : group.spawn_positions) group.harvest_near_tiles.draw_chebyshev(tile, kHarvestNearRadius);
                group.harvest_near_ready = true;
            }
            const auto better = std::find_if(visible_allies.begin(), visible_allies.end(), [&](const auto& ally) {
                return ally.rank < my_rank && group.harvest_near_tiles.is_set(ally.position);
            });
            if (better != visible_allies.end()) {
                group.reward_waiting = false;
                group.yield_until_round = round + kYieldCaptureRounds;
                group.respawn_ready_round = round + kYieldCaptureRounds + static_cast<std::uint32_t>(group.max_respawn);
            }
        }
    }

    // Move off only when a visible, timer-zero camp is blocked from spawning.
    const auto observed_position = me.position();
    for (const auto& group : match.camp_groups) {
        if (group.blocked_round != round || !group.spawn_tiles.is_set(observed_position)) continue;
        nav::move_off(memory.movement, group.spawn_tiles);
        break;
    }

    if (field_round != round) {
        field_round = round;
        cambc::bitboard visited;
        cambc::bitboard roots;
        cached_self_position = me.position();
        roots.set(*cached_self_position);
        field.compute(match.static_blockers, roots, visited, farm_constants::kCampSearchRange);
    }

    const float walk_factor = (profile.is_ranged || memory.movement.has_dash)
        ? farm_constants::kRespawnWalkFactor : 1.0f;
    const auto best = select_best_camp(match, field, dps, round, walk_factor);
    if (!best) {
        return;
    }

    auto& group = match.camp_groups[static_cast<std::size_t>(*best)];

    if (group.reward_waiting) {
        const auto position = me.position();
        if (group.spawn_tiles.is_set(position)) return;
        cambc::bitboard occupied;
        for (const auto& ally : visible_allies) occupied.set(ally.position);
        std::optional<cambc::vec2> destination;
        int nearest = kUnreached;
        for (const auto tile : group.spawn_positions) {
            if (occupied.is_set(tile)) continue;
            const int distance = field.get(tile);
            if (distance < nearest) { nearest = distance; destination = tile; }
        }
        if (destination) nav::move_towards(memory.movement, *destination, true, true);
        else {
            beside_tiles(match, group);
            if (!group.beside_tiles.is_set(position) && !group.beside_positions.empty())
                nav::move_towards(memory.movement, group.beside_positions.front(), true, false);
        }
        return;
    }

    std::vector<const cambc::camp*> candidates;
    std::vector<CampThreat> camp_threats;
    for (const auto& camp : *group.view) {
        try {
            if (!camp.is_visible() || camp.health() <= 0) continue;
            candidates.push_back(&camp);
            if (heart_shield_can_cast) {
                camp_threats.push_back({camp.position(), camp.attack_range(), camp.strength() > 0.0f});
            }
        } catch (const cambc::error&) {
            // Hidden camps are not actionable.
        }
    }
    if (candidates.empty()) {
        if (respawn_remaining(group, round) > 0) {
            beside_tiles(match, group);
            const auto position = cached_self_position ? *cached_self_position : me.position();
            if (!group.beside_tiles.is_set(position)) {
                std::optional<cambc::vec2> destination;
                int shortest_distance = kUnreached;
                for (const auto tile : group.beside_positions) {
                    const int distance = field.get(tile);
                    if (distance < shortest_distance) {
                        shortest_distance = distance;
                        destination = tile;
                    }
                }
                if (destination) nav::move_towards(memory.movement, *destination, true, false);
            }
        } else {
            // Unknown, hidden, reward-available, or expired: keep the established fallback.
            nav::move_towards(memory.movement, group.centre, true, true);
        }
        return;
    }

    bool ability_cast = false;
    bool ability_exclusive_movement = false;
    bool position_changed = false;

    if (heart_shield_can_cast) {
        if (!cached_self_position) cached_self_position = me.position();
        const auto position = *cached_self_position;
        const bool threatened = std::any_of(camp_threats.begin(), camp_threats.end(), [&](const CampThreat& camp) {
            return camp.damaging && position.chebyshev_distance(camp.position) <= camp.attack_range;
        });
        if (threatened) {
            try {
                ability_cast = (*heart_abilities)[0].use();
            } catch (const cambc::error&) {
                // A failed shield cast leaves the turn's cast available.
            }
        }
    }
    auto my_pos = cached_self_position ? *cached_self_position : me.position();
    const auto attack_tiles = me.attack_tiles();
    const cambc::camp* preferred_target = nullptr;
    const cambc::camp* attack_target = best_attack_target(candidates, attack_tiles, my_pos);
    for (const auto* camp : candidates) {
        if (!preferred_target || is_better_candidate(*camp, *preferred_target, my_pos)) {
            preferred_target = camp;
        }
    }
    if (me.can_cast() && !ability_cast) {
        cambc::ability* best_ability = nullptr;
        const cambc::camp* best_target = nullptr;
        cambc::vec2 best_destination{};
        bool best_uses_destination = false;
        float best_damage = 0.0f;
        auto abilities = heart_abilities ? *heart_abilities : me.abilities();

        for (std::size_t slot = 0; slot < abilities.size(); ++slot) {
            auto& ability = abilities[slot];
            try {
                if (!ability.is_ready()) {
                    continue;
                }
                const bool direct_damage = ability.is_damage();
                const bool area_damage = ability.is_aoe_damage();
                const bool cc_damage = ability.is_cc() && ability.get<cambc::ability_cc>().modifier > 0.0f;
                const bool dash_damage = ability.is_dash() && ability.get<cambc::ability_dash>().modifier > 0.0f;
                if (!direct_damage && !area_damage && !cc_damage && !dash_damage) {
                    continue;
                }
                if (dash_damage && memory.movement.cooldown > 0) {
                    continue;
                }

                const auto valid_tiles = ability.valid_tiles();
                const float damage = ability.damage_amount();
                if (damage <= 0.0f) {
                    continue;
                }

                const cambc::camp* target = nullptr;
                cambc::vec2 destination{};
                bool uses_destination = false;
                if (dash_damage) {
                    const auto dash = ability.get<cambc::ability_dash>();
                    cambc::bitboard legal_landings;
                    nav::filter_legal_dash_landings(memory.movement, my_pos, valid_tiles, legal_landings);
                    const auto landing_positions = legal_landings.positions();
                    std::vector<const cambc::camp*> dash_targets;
                    dash_targets.reserve(candidates.size());
                    const auto maximum_camp_distance = static_cast<std::uint32_t>(dash.radius + 2);
                    for (const auto* camp : candidates) {
                        if (camp->position().chebyshev_distance(my_pos) <= maximum_camp_distance) {
                            dash_targets.push_back(camp);
                        }
                    }
                    std::sort(dash_targets.begin(), dash_targets.end(), [&](const auto* a, const auto* b) {
                        return is_better_candidate(*a, *b, my_pos);
                    });
                    for (const auto* camp : dash_targets) {
                        const auto camp_position = camp->position();
                        int nearest_destination = 1 << 20;
                        for (const auto tile : landing_positions) {
                            if (tile == camp_position || tile.chebyshev_distance(camp_position) > dash.radius) continue;
                            const int distance = static_cast<int>(tile.chebyshev_distance(my_pos));
                            if (distance < nearest_destination) {
                                target = camp;
                                destination = tile;
                                uses_destination = true;
                                nearest_destination = distance;
                            }
                        }
                        if (target == camp) break;
                    }
                } else {
                    for (const auto* camp : candidates) {
                        if (valid_tiles.is_set(camp->position()) &&
                            (!target || is_better_candidate(*camp, *target, my_pos))) {
                            target = camp;
                        }
                    }
                }

                if (target && (!best_target || damage > best_damage)) {
                    best_damage = damage;
                    best_ability = &ability;
                    best_target = target;
                    best_destination = destination;
                    best_uses_destination = uses_destination;
                }
            } catch (const cambc::error&) {
                continue;
            }
        }
        if (best_ability && best_target) {
            try {
                ability_cast = best_uses_destination
                    ? best_ability->use(best_destination)
                    : best_ability->use(*best_target);
                position_changed = ability_cast && best_uses_destination;
                if (ability_cast && best_ability->is_dash()) memory.movement.cooldown = kDashCooldown;
                if (ability_cast) note_kill_cooldown_reset(memory, *best_target);
                if (ability_cast) {
                    std::visit([&ability_exclusive_movement](auto&& payload) {
                        using T = std::decay_t<decltype(payload)>;
                        if constexpr (std::is_same_v<T, cambc::ability_properties>) {
                            ability_exclusive_movement = payload.exclusive_with_movement;
                        }
                    }, best_ability->info().value());
                }
            } catch (const cambc::error&) {
                // A failed ability cast leaves movement and attacks available.
            }
        }
    }
    if (me.can_move() && !ability_exclusive_movement) {
        bool should_move;
        if (profile.is_ranged && !farm_constants::kRangedWalkToPriority) {
            should_move = attack_target == nullptr;
        } else {
            should_move = !attack_tiles.is_set(preferred_target->position());
        }
        if (should_move) {
            position_changed = nav::move_towards(
                memory.movement, preferred_target->position(), attack_target == nullptr, true);
        }
    }
    if (position_changed) my_pos = me.position();
    const auto current_attack_tiles = position_changed ? me.attack_tiles() : attack_tiles;
    attack_target = best_attack_target(candidates, current_attack_tiles, my_pos);
    if (me.can_attack() && attack_target) {
        try {
            me.attack(*attack_target);
            note_kill_cooldown_reset(memory, *attack_target);

            // A reset ability can only be used if this turn's spell cast is still available.
            if (!ability_cast) {
                auto abilities = me.abilities();
                for (std::size_t slot = 0; slot < abilities.size(); ++slot) {
                    auto& ability = abilities[slot];
                    const bool reset = ability.is_attack_reset();
                    const bool ready = reset && ability.is_ready();
                    if (!ready) continue;
                    bool reset_used = false;
                    try {
                        reset_used = ability.use();
                    } catch (const cambc::error&) {
                    }
                    if (reset_used) {
                        const auto reset_attack_tiles = me.attack_tiles();
                        const auto second_target = best_attack_target(candidates, reset_attack_tiles, my_pos);
                        if (me.can_attack() && second_target) {
                            try {
                                me.attack(*second_target);
                                note_kill_cooldown_reset(memory, *second_target);
                            } catch (const cambc::error&) {
                            }
                        }
                    }
                    break;
                }
            }
        } catch (const cambc::error&) {
        }
    }
    return;
}
