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

struct CampCandidate {
    const cambc::camp* view;
    cambc::camp_type kind;
    cambc::vec2 position;
    float health;
    int priority_rank;
    int distance;
};

void observe_respawns(MatchData& match, std::uint32_t now) {
    if (match.respawn_observed_round == now) return;
    cambc::bitboard visible_tiles;
    cambc::bitboard live_camp_tiles;
    cambc::vision::visible_tiles_into(visible_tiles);
    cambc::vision::camp_tiles_into(live_camp_tiles);
    for (auto& group : match.camp_groups) {
        ensure_spawn_data(group);
        for (auto& camp : group.camp_threats) {
            if (visible_tiles.is_set(camp.position)) {
                camp.alive = live_camp_tiles.is_set(camp.position);
            }
        }
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

void refresh_respawn_beliefs(MatchData& match, std::uint32_t now) {
    for (auto& group : match.camp_groups) {
        if (!group.max_respawn_cached && group.view) {
            group.max_respawn = group.view->max_respawn_timer();
            group.max_respawn_cached = true;
        }
        if (group.reward_waiting && now - group.reward_seen_round >= kRewardBeliefRounds) {
            group.reward_waiting = false;
            group.respawn_ready_round = group.reward_seen_round + kRewardBeliefRounds +
                static_cast<std::uint32_t>(group.max_respawn);
        }
    }
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

// Check if champion has Capture emblem (caps the team's remaining capture time at 3 rounds).
bool has_capture_emblem() {
    try {
        auto& me = cambc::self();
        const auto counts = me.item_count();
        if (counts[cambc::item_type::capture_emblem_of_strength] > 0) return true;
        if (counts[cambc::item_type::capture_emblem_of_focus] > 0) return true;
        if (counts[cambc::item_type::capture_emblem_of_health] > 0) return true;
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

bool is_better_candidate(const CampCandidate& a, const CampCandidate& b) {
    if (a.priority_rank != b.priority_rank) return a.priority_rank < b.priority_rank;
    if (a.distance != b.distance) return a.distance < b.distance;
    if (a.health != b.health) return a.health < b.health;
    if (a.position.y != b.position.y) return a.position.y < b.position.y;
    return a.position.x < b.position.x;
}

bool is_better_attack_candidate(const CampCandidate& a, float health_a,
                                const CampCandidate& b, float health_b,
                                const cambc::vec2& my_pos) {
    if (a.priority_rank != b.priority_rank) return a.priority_rank < b.priority_rank;
    const int distance_a = static_cast<int>(a.position.chebyshev_distance(my_pos));
    const int distance_b = static_cast<int>(b.position.chebyshev_distance(my_pos));
    if (distance_a != distance_b) return distance_a < distance_b;
    if (health_a != health_b) return health_a < health_b;
    if (a.position.y != b.position.y) return a.position.y < b.position.y;
    return a.position.x < b.position.x;
}

const CampCandidate* best_attack_target(const std::vector<CampCandidate>& candidates,
                                        const cambc::bitboard& attack_tiles,
                                        const cambc::vec2& my_pos,
                                        const CampCandidate* preferred_target = nullptr) {
    if (preferred_target && attack_tiles.is_set(preferred_target->position)) {
        try {
            if (preferred_target->view->health() > 0) {
                return preferred_target;
            }
        } catch (const cambc::error&) {
            // Continue with the remaining live candidates.
        }
    }

    const CampCandidate* best = nullptr;
    float best_health = 0.0f;
    for (const auto& camp : candidates) {
        try {
            if (!attack_tiles.is_set(camp.position)) continue;
            const float health = camp.view->health();
            if (health <= 0) continue;
            if (!best || is_better_attack_candidate(camp, health, *best, best_health, my_pos)) {
                best = &camp;
                best_health = health;
            }
        } catch (const cambc::error&) {
            continue;
        }
    }
    return best;
}

bool surrounded_by_live_camps(cambc::vec2 position, const cambc::bitboard& live_camp_tiles) {
    // For melee movement priority, exclude a camp with no open neighboring tile.
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            const cambc::vec2 neighbor{position.x + dx, position.y + dy};
            if (!cambc::map::is_in_bounds(neighbor)) continue;
            if (!live_camp_tiles.is_set(neighbor)) return false;
        }
    }
    return true;
}

}

bool retreat_to_spawn(BotMemory& memory, MatchData& match, bool include_enemy_threats) {
    auto& me = cambc::self();
    if (cambc::map::spawn_region(cambc::team::ally).is_set(me.position())) {

        return true;
    }

    cambc::bitboard danger_tiles;
    const auto round = cambc::round_number();
    for (const auto& group : match.camp_groups) {
        const bool known_inactive = group.reward_waiting || group.respawn_ready_round > round ||
            (group.blocked_round != std::numeric_limits<std::uint32_t>::max() &&
             round >= group.blocked_round && round - group.blocked_round < farm_constants::kBlockedSkipRounds);
        if (known_inactive) continue;
        for (const auto& camp : group.camp_threats) {
            const bool respawn_due = group.respawn_ready_round > 0 && group.respawn_ready_round <= round;
            if (!camp.alive && !respawn_due) continue;
            danger_tiles.draw_chebyshev(camp.position, static_cast<float>(camp.attack_range));
        }
    }

    if (include_enemy_threats) {
        for (const auto& enemy : visible_enemies_for_round(match)) {
            try {
                if (!enemy.is_alive()) continue;
                const auto position = enemy.position();
                const auto strength = enemy.strength();
                if (strength > 0.0f) {
                    danger_tiles.draw_chebyshev(position, static_cast<float>(enemy.attack_range() + 1));
                }

                for (const auto& ability : enemy.abilities()) {
                    float range = 0.0f;
                    float radius = 0.0f;
                    bool damaging = false;
                    if (ability.is_damage()) {
                        range = ability.get<cambc::ability_damage>().range;
                        damaging = true;
                    } else if (ability.is_aoe_damage()) {
                        const auto info = ability.get<cambc::ability_aoe_damage>();
                        range = info.range;
                        radius = info.radius;
                        damaging = true;
                    } else if (ability.is_cc()) {
                        const auto info = ability.get<cambc::ability_cc>();
                        range = info.range;
                        damaging = info.modifier > 0.0f;
                    } else if (ability.is_dash()) {
                        const auto info = ability.get<cambc::ability_dash>();
                        range = info.range;
                        radius = info.radius;
                        damaging = info.modifier > 0.0f;
                    }
                    if (damaging) {
                        danger_tiles.draw_chebyshev(position, range + radius + 1.0f);
                    }
                }
            } catch (const cambc::error&) {
                // Ignore enemies or abilities that cease to be queryable this turn.
            }
        }
    }

    auto position = me.position();

    if (!danger_tiles.is_set(position)) {
        try {
            me.recall();
            memory.recall_channel_started = true;
            memory.tribute_altar.reset();
            memory.pursuit_target.reset();
            memory.abandoned_pursuit_target.reset();
            memory.abandoned_pursuit_last_health = 0.0f;
            memory.abandoned_pursuit_last_seen_round = std::numeric_limits<std::uint32_t>::max();
            memory.pursuit_last_damage_round = 0;

        } catch (const cambc::error&) {

            // Stay out of camp range and retry on a later turn.
        }
        return true;
    }

    cambc::bitboard safe_candidates;
    // Dilate the dense danger mask with whole-board shifts. This is the same
    // radius-1 Chebyshev neighborhood as drawing around every set tile, but
    // has fixed cost instead of one SDK draw call per danger tile.
    cambc::bitboard horizontal_span = danger_tiles;
    cambc::bitboard shifted;
    danger_tiles.shift_right_into(-1, shifted);
    horizontal_span |= shifted;
    danger_tiles.shift_right_into(1, shifted);
    horizontal_span |= shifted;

    safe_candidates = horizontal_span;
    horizontal_span.shift_up_into(-1, shifted);
    safe_candidates |= shifted;
    horizontal_span.shift_up_into(1, shifted);
    safe_candidates |= shifted;
    safe_candidates &= ~danger_tiles;
    safe_candidates &= ~match.static_blockers;

    cambc::bitboard occupied_allies;
    cambc::bitboard occupied_enemies;
    nav::refresh_visible_champion_tiles(occupied_allies, occupied_enemies);
    safe_candidates &= ~(occupied_allies | occupied_enemies);

    cambc::bitboard roots;
    roots.set(position);
    cambc::bitboard visited;
    cambc::distance_field field;
    field.compute_to(match.static_blockers, roots, safe_candidates, visited);

    std::optional<cambc::vec2> destination;
    int nearest = kUnreached;
    for (const auto tile : safe_candidates.positions()) {
        const int distance = field.get(tile);
        if (distance != kUnreached && distance < nearest) {
            nearest = distance;
            destination = tile;
        }
    }
    if (!destination) {

        return true;
    }

    const bool moved = nav::move_towards(memory.movement, *destination, true, false);

    if (!moved) return true;
    position = me.position();
    if (!danger_tiles.is_set(position)) {
        try {
            me.recall();
            memory.recall_channel_started = true;
            memory.tribute_altar.reset();
            memory.pursuit_target.reset();
            memory.abandoned_pursuit_target.reset();
            memory.abandoned_pursuit_last_health = 0.0f;
            memory.abandoned_pursuit_last_seen_round = std::numeric_limits<std::uint32_t>::max();
            memory.pursuit_last_damage_round = 0;

        } catch (const cambc::error&) {

            // Retry recall from safety on the next turn.
        }
    }
    return true;
}

bool recall_turn(MatchData& match, BotMemory& memory, bool force_recall, float active_fight_distance) {
    auto& me = cambc::self();
    const auto position = me.position();
    const bool at_spawn = cambc::map::spawn_region(cambc::team::ally).is_set(position);
    const bool low_health = me.health() <= me.max_health() * 0.35f;
    const auto& enemies = visible_enemies_for_round(match);
    bool active_fight = false;
    for (const auto& enemy : enemies) {
        try {
            if (!enemy.is_alive()) continue;
            if (enemy.position().chebyshev_distance(position) <= active_fight_distance ||
                (memory.pursuit_target && static_cast<cambc::handle_champion_t>(enemy) == *memory.pursuit_target)) {
                active_fight = true;
                break;
            }
        } catch (const cambc::error&) { }
    }

    const bool shop_recall = !active_fight && !at_spawn && me.gold_count() >= 300;

    if (force_recall && at_spawn && me.is_submitting_gems()) {
        try { me.cancel_submit_gems(); } catch (const cambc::error&) { }
        return true;
    }
    if (force_recall && at_spawn) return false;
    if (!force_recall && !low_health && !shop_recall) {

        return false;
    }

    if (!at_spawn && me.is_submitting_gems()) {
        try {
            me.cancel_submit_gems();

        } catch (const cambc::error&) {

            return true;
        }
    }
    if (!at_spawn) {
        const auto round = cambc::round_number();
        observe_respawns(match, round);
        refresh_respawn_beliefs(match, round);
    }
    const bool result = retreat_to_spawn(memory, match, low_health && active_fight);

    return result;
}

void farm_turn(const ChampionProfile& profile, MatchData& match, BotMemory& memory,
               std::optional<cambc::vec2> coordinated_target,
               cambc::vec2* selected_target) {
    auto& me = cambc::self();
    std::optional<decltype(me.abilities())> heart_abilities;
    bool heart_shield_can_cast = false;

    std::optional<cambc::vec2> cached_self_position;
    static cambc::distance_field field;
    static std::uint32_t field_round = 0;
    const auto round = cambc::round_number();

    observe_respawns(match, round);
    refresh_respawn_beliefs(match, round);
    const bool low_health = me.health() <= me.max_health() * 0.35f;

    // Move off only when a visible, timer-zero camp is blocked from spawning.
    if (!low_health) {
        const auto observed_position = me.position();
        for (const auto& group : match.camp_groups) {
            if (group.blocked_round != round || !group.spawn_tiles.is_set(observed_position)) continue;
            nav::move_off(memory.movement, group.spawn_tiles);

            break;
        }
    }

    if (field_round != round) {
        field_round = round;
        cambc::bitboard visited;
        cambc::bitboard roots;
        cached_self_position = me.position();
        roots.set(*cached_self_position);
        field.compute(match.static_blockers, roots, visited, farm_constants::kCampSearchRange);
    }

    if (memory.movement.is_heart_of_ruin) {
        heart_abilities.emplace(me.abilities());
        auto& shield = (*heart_abilities)[0];
        heart_shield_can_cast = shield.is_ready() && shield.is_shield() && me.can_cast();
    }

    const float dps = calculate_dps(profile);
    bool has_reward_belief = false;
    for (auto& group : match.camp_groups) {
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

    const float walk_factor = (profile.is_ranged || memory.movement.has_dash)
        ? farm_constants::kRespawnWalkFactor : 1.0f;
    std::optional<int> best;
    if (coordinated_target) {
        const auto found = std::find_if(match.camp_groups.begin(), match.camp_groups.end(), [&](const auto& group) {
            return group.centre == *coordinated_target;
        });
        if (found == match.camp_groups.end()) return;
        best = static_cast<int>(std::distance(match.camp_groups.begin(), found));
    } else {
        best = select_best_camp(match, field, dps, round, walk_factor);
    }
    if (!best) {

        return;
    }

    auto& group = match.camp_groups[static_cast<std::size_t>(*best)];
    if (selected_target) *selected_target = group.centre;

    if (group.reward_waiting) {
        const auto position = me.position();
        if (group.spawn_tiles.is_set(position)) {

            return;
        }
        cambc::bitboard occupied;
        for (const auto& ally : visible_allies) occupied.set(ally.position);
        std::optional<cambc::vec2> destination;
        int nearest = kUnreached;
        for (const auto tile : group.spawn_positions) {
            if (occupied.is_set(tile)) continue;
            const int distance = field.get(tile);
            if (distance < nearest) { nearest = distance; destination = tile; }
        }
        if (destination) {
            nav::move_towards(memory.movement, *destination, true, true);

        }
        else {
            beside_tiles(match, group);
            if (!group.beside_tiles.is_set(position) && !group.beside_positions.empty()) {
                const auto fallback = group.beside_positions.front();
                nav::move_towards(memory.movement, fallback, true, false);

            }
        }

        return;
    }

    const auto candidate_origin = cached_self_position ? *cached_self_position : me.position();
    auto my_pos = candidate_origin;
    std::vector<CampCandidate> candidates;
    std::vector<CampThreat> camp_threats;
    for (const auto& camp : *group.view) {
        try {
            if (!camp.is_visible()) continue;
            const auto health = camp.health();
            if (health <= 0) continue;
            const auto position = camp.position();
            const auto kind = camp.kind();
            candidates.push_back({
                &camp,
                kind,
                position,
                health,
                farm_constants::get_camp_priority_rank(kind),
                static_cast<int>(position.chebyshev_distance(candidate_origin)),
            });
            if (heart_shield_can_cast) {
                camp_threats.push_back({position, camp.attack_range(), camp.strength() > 0.0f});
            }
        } catch (const cambc::error&) {
            // Hidden camps are not actionable.
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return is_better_candidate(a, b);
    });
    cambc::bitboard live_camp_tiles;
    for (const auto& camp : candidates) live_camp_tiles.set(camp.position);
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
                if (destination) {
                    nav::move_towards(memory.movement, *destination, true, false);

                }
            }
        } else {
            // Unknown, hidden, reward-available, or expired: keep the established fallback.
            nav::move_towards(memory.movement, group.centre, true, true);

        }

        return;
    }

    // Select the first ranked camp with an open neighboring tile for melee.
    // Keep the full list for post-move attacks and abilities.
    const CampCandidate* preferred_target = profile.is_ranged ? &candidates.front() : nullptr;
    if (!profile.is_ranged) {
        for (const auto& candidate : candidates) {
            const bool surrounded = surrounded_by_live_camps(candidate.position, live_camp_tiles);
            if (!surrounded) {
                preferred_target = &candidate;
                break;
            }
        }
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
    const auto attack_tiles = me.attack_tiles();
    const CampCandidate* attack_target = best_attack_target(
        candidates, attack_tiles, my_pos, profile.is_ranged ? nullptr : preferred_target);
    if (me.can_cast() && !ability_cast) {
        cambc::ability* best_ability = nullptr;
        const CampCandidate* best_target = nullptr;
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

                const CampCandidate* target = nullptr;
                cambc::vec2 destination{};
                bool uses_destination = false;
                if (dash_damage) {
                    const auto dash = ability.get<cambc::ability_dash>();
                    cambc::bitboard legal_landings;
                    nav::filter_legal_dash_landings(memory.movement, my_pos, valid_tiles, legal_landings);
                    const auto landing_positions = legal_landings.positions();
                    const auto maximum_camp_distance = static_cast<std::uint32_t>(dash.radius + 2);
                    for (const auto& camp : candidates) {
                        if (camp.distance > static_cast<int>(maximum_camp_distance)) continue;
                        const auto camp_position = camp.position;
                        int nearest_destination = 1 << 20;
                        for (const auto tile : landing_positions) {
                            if (tile == camp_position || tile.chebyshev_distance(camp_position) > dash.radius) continue;
                            if (memory.movement.movement_guard_center &&
                                tile.chebyshev_distance(*memory.movement.movement_guard_center) >
                                    static_cast<std::uint32_t>(memory.movement.movement_guard_radius)) continue;
                            const int distance = static_cast<int>(tile.chebyshev_distance(my_pos));
                            if (distance < nearest_destination) {
                                target = &camp;
                                destination = tile;
                                uses_destination = true;
                                nearest_destination = distance;
                            }
                        }
                        if (target == &camp) break;
                    }
                } else {
                    for (const auto& camp : candidates) {
                        if (valid_tiles.is_set(camp.position)) {
                            target = &camp;
                            break;
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
                    : best_ability->use(*best_target->view);

                position_changed = ability_cast && best_uses_destination;
                if (ability_cast && best_ability->is_dash()) memory.movement.cooldown = kDashCooldown;
                if (ability_cast) note_kill_cooldown_reset(memory, *best_target->view);
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
        } else if (preferred_target) {
            should_move = !attack_tiles.is_set(preferred_target->position);
        } else {
            should_move = false;
        }
        if (should_move) {
            if (!profile.is_ranged) {
                // A failed SDK move is diagnostic; do not try lower-ranked targets.
                position_changed = nav::move_towards(
                    memory.movement, preferred_target->position, attack_target == nullptr, true);
            } else {
                for (const auto& candidate : candidates) {
                    position_changed = nav::move_towards(
                        memory.movement, candidate.position, attack_target == nullptr, true);
                    if (position_changed) break;
                }
            }
        }
    }
    if (position_changed) my_pos = me.position();
    const auto current_attack_tiles = position_changed ? me.attack_tiles() : attack_tiles;
    attack_target = best_attack_target(
        candidates, current_attack_tiles, my_pos, profile.is_ranged ? nullptr : preferred_target);
    if (me.can_attack() && attack_target) {
        try {
            me.attack(*attack_target->view);

            note_kill_cooldown_reset(memory, *attack_target->view);

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
                                me.attack(*second_target->view);

                                note_kill_cooldown_reset(memory, *second_target->view);
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

bool handle_cleared_coordinated_target(MatchData& match, BotMemory& memory,
                                       cambc::vec2 target, cambc::vec2 prism_position) {
    if (target.x < 0 || target.x >= 100 || target.y < 0 || target.y >= 100 ||
        prism_position.x < 0 || prism_position.x >= 100 ||
        prism_position.y < 0 || prism_position.y >= 100) return false;

    const auto found = std::find_if(match.camp_groups.begin(), match.camp_groups.end(), [&](const auto& group) {
        return group.centre == target;
    });
    if (found == match.camp_groups.end()) return false;

    const auto round = cambc::round_number();
    observe_respawns(match, round);
    refresh_respawn_beliefs(match, round);
    const bool recently_blocked = found->blocked_round != std::numeric_limits<std::uint32_t>::max() &&
        round >= found->blocked_round &&
        round - found->blocked_round < farm_constants::kBlockedSkipRounds;
    const bool cleared = found->reward_waiting || found->respawn_ready_round > round || recently_blocked;
    if (!cleared) return false;

    const auto& camp_tiles = all_camp_spawn_tiles(match);
    auto& movement = memory.movement;
    const auto position = cambc::self().position();
    movement.movement_guard_center.reset();
    if (camp_tiles.is_set(position)) {
        // Prefer one safe neighboring tile; if enclosed, search outward from ring 2.
        if (nav::move_off(movement, camp_tiles)) return true;
        if (!cambc::self().can_move()) return true;

        const auto move_toward_first_safe_ring = [&] {
            for (int radius = 2; radius < 100; ++radius) {
                const auto consider = [&](cambc::vec2 tile) {
                    if (!cambc::map::is_in_bounds(tile) || camp_tiles.is_set(tile) ||
                        match.static_blockers.is_set(tile) ||
                        movement.scratch.ally_tiles.is_set(tile) ||
                        movement.scratch.enemy_tiles.is_set(tile)) return false;
                    nav::move_towards(movement, tile, false, false);
                    return true;
                };

                for (int offset = -radius; offset <= radius; ++offset) {
                    if (consider({position.x + offset, position.y - radius}) ||
                        consider({position.x + offset, position.y + radius})) return;
                }
                for (int offset = -radius + 1; offset < radius; ++offset) {
                    if (consider({position.x - radius, position.y + offset}) ||
                        consider({position.x + radius, position.y + offset})) return;
                }
            }
        };
        move_toward_first_safe_ring();
        return true;
    }

    constexpr int kPrismTetherRadius = 3;
    if (position.chebyshev_distance(prism_position) > kPrismTetherRadius) {
        nav::move_into_radius(movement, prism_position, kPrismTetherRadius, true, &camp_tiles);
        return true;
    }

    movement.movement_guard_center = prism_position;
    movement.movement_guard_radius = kPrismTetherRadius;
    nav::move_randomly_within_radius(movement, prism_position, kPrismTetherRadius, camp_tiles);
    return true;
}
