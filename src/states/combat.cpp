#include "combat.h"
#include "farm.h"
#include "../core/match_data.h"
#include "../nav/pathfinding.h"
#include "../champions/registry.h"
#include <cambc.h>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace {
constexpr std::uint32_t kPursuitLimit = 5;

struct Enemy {
    cambc::champion view;
    cambc::handle_champion_t handle;
    cambc::vec2 position;
    float health;
    float damage_health_ratio;
    bool in_spawn_zone;
};

std::optional<cambc::vec2> own_spawn_destination(const MatchData& match,
                                                 const cambc::vec2& my_position) {
    std::optional<cambc::vec2> destination;
    std::int64_t best_distance = std::numeric_limits<std::int64_t>::max();
    for (const auto tile : match.ally_spawn_positions) {
        const auto distance = my_position.chebyshev_distance(tile);
        if (!destination || distance < best_distance) {
            destination = tile;
            best_distance = distance;
        }
    }
    return destination;
}

void retreat_toward_own_spawn(MatchData& match, BotMemory& memory, const cambc::vec2& my_position,
                              cambc::vec2* target_position) {
    const auto destination = own_spawn_destination(match, my_position);
    if (!destination) return;
    if (target_position) *target_position = *destination;
    nav::move_towards(memory.movement, *destination, true, false);
}

bool is_better_target(const Enemy& a, const Enemy& b, const cambc::vec2& my_position) {
    if (a.damage_health_ratio != b.damage_health_ratio)
        return a.damage_health_ratio > b.damage_health_ratio;
    const auto distance_a = a.position.chebyshev_distance(my_position);
    const auto distance_b = b.position.chebyshev_distance(my_position);
    if (distance_a != distance_b) return distance_a < distance_b;
    if (a.health != b.health) return a.health < b.health;
    if (a.position.y != b.position.y) return a.position.y < b.position.y;
    return a.position.x < b.position.x;
}

void clear_pursuit(BotMemory& memory) {
    memory.pursuit_target.reset();
    memory.pursuit_last_damage_round = 0;
}

void clear_abandoned_pursuit(BotMemory& memory) {
    memory.abandoned_pursuit_target.reset();
    memory.abandoned_pursuit_last_health = 0.0f;
    memory.abandoned_pursuit_last_seen_round = std::numeric_limits<std::uint32_t>::max();
}

void note_damage(BotMemory& memory, Enemy& target, std::uint32_t round) {
    try {
        const auto health = target.view.health();
        if (health < target.health) memory.pursuit_last_damage_round = round;
        target.health = health;
    } catch (const cambc::error&) { }
}

float estimated_dps(const cambc::champion& champion, const ChampionProfile* profile) {
    const auto strength = champion.strength();
    const auto focus = champion.focus();
    float dps = strength;
    if (!profile) return dps;
    if (profile->ability1_is_attack_reset) dps += strength;
    else if (profile->ability1_damage_multiplier > 0.0f)
        dps += profile->ability1_damage_multiplier * focus;
    if (profile->ability2_is_attack_reset) dps += strength;
    else if (profile->ability2_damage_multiplier > 0.0f)
        dps += profile->ability2_damage_multiplier * focus;
    return dps;
}

std::optional<cambc::vec2> best_dash_landing(BotMemory& memory,
                                             const cambc::bitboard& valid_tiles,
                                             const cambc::vec2& target,
                                             float radius,
                                             const cambc::vec2& my_position) {
    cambc::bitboard legal_landings;
    nav::filter_legal_dash_landings(memory.movement, my_position, valid_tiles, legal_landings);
    std::optional<cambc::vec2> best;
    std::uint32_t best_target_distance = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t best_travel_distance = std::numeric_limits<std::uint32_t>::max();
    for (const auto tile : legal_landings.positions()) {
        const auto target_distance = tile.chebyshev_distance(target);
        if (target_distance > radius) continue;
        const auto travel_distance = tile.chebyshev_distance(my_position);
        if (!best || target_distance < best_target_distance ||
            (target_distance == best_target_distance && travel_distance < best_travel_distance)) {
            best = tile;
            best_target_distance = target_distance;
            best_travel_distance = travel_distance;
        }
    }
    return best;
}

bool is_damage_dealing_ability(cambc::ability& ability) {
    return ability.is_damage() || ability.is_aoe_damage() ||
        (ability.is_cc() && ability.get<cambc::ability_cc>().modifier > 0.0f) ||
        (ability.is_dash() && ability.get<cambc::ability_dash>().modifier > 0.0f);
}

bool can_damage_target_now(Enemy& target, BotMemory& memory, const cambc::vec2& my_position) {
    auto& me = cambc::self();
    if (me.can_attack() && me.attack_tiles().is_set(target.position)) return true;
    if (!me.can_cast()) return false;

    auto abilities = me.abilities();
    for (std::size_t slot = 0; slot < abilities.size(); ++slot) {
        auto& ability = abilities[slot];
        try {
            if (!ability.is_ready()) continue;
            if (!is_damage_dealing_ability(ability)) continue;
            if (ability.is_dash() && memory.movement.cooldown > 0) continue;
            if (ability.damage_amount() <= 0.0f) continue;

            const auto targets = ability.valid_targets();
            if (ability.is_dash()) {
                if (!targets.contains(cambc::ability_target_mask::ground)) continue;
            } else if (!targets.contains(cambc::ability_target_mask::enemies)) {
                continue;
            }

            const auto valid_tiles = ability.valid_tiles();
            if (ability.is_dash()) {
                const auto dash = ability.get<cambc::ability_dash>();
                if (!best_dash_landing(memory, valid_tiles, target.position,
                                       dash.radius, my_position)) continue;
            } else if (!valid_tiles.is_set(target.position)) {
                continue;
            }

            if (ability.damage_amount(target.view) > 0.0f) return true;
        } catch (const cambc::error&) {
            continue;
        }
    }
    return false;
}

void fight_target(Enemy& target, BotMemory& memory, std::uint32_t round) {
    auto& me = cambc::self();
    const auto my_position = me.position();
    bool ability_used = false;

    if (me.can_cast()) {
        auto abilities = me.abilities();
        if (memory.movement.is_heart_of_ruin) {
            auto& shield = abilities[0];
            const bool shield_ready = shield.is_ready() && shield.is_shield();
            bool threatened = shield_ready && target.view.strength() > 0.0f &&
                my_position.chebyshev_distance(target.position) <= target.view.attack_range();
            if (shield_ready && !threatened) {
                for (const auto& camp : cambc::vision::camps()) {
                    try {
                        if (camp.health() > 0.0f && camp.strength() > 0.0f &&
                            my_position.chebyshev_distance(camp.position()) <= camp.attack_range()) {
                            threatened = true;
                            break;
                        }
                    } catch (const cambc::error&) { }
                }
            }
            if (threatened) {
                try { ability_used = shield.use(); }
                catch (const cambc::error&) { }
            }
        }

        cambc::ability* best_ability = nullptr;
        float best_damage = 0.0f;
        std::optional<cambc::vec2> best_landing;
        for (std::size_t slot = 0; !ability_used && slot < abilities.size(); ++slot) {
            auto& ability = abilities[slot];
            try {
                if (!ability.is_ready()) continue;
                if (!is_damage_dealing_ability(ability)) continue;
                if (ability.is_dash() && memory.movement.cooldown > 0) continue;
                const auto damage = ability.damage_amount();
                if (damage <= best_damage) continue;
                const auto valid_tiles = ability.valid_tiles();
                std::optional<cambc::vec2> landing;
                if (ability.is_dash()) {
                    const auto dash = ability.get<cambc::ability_dash>();
                    landing = best_dash_landing(memory, valid_tiles, target.position, dash.radius, my_position);
                    if (!landing) continue;
                } else if (!valid_tiles.is_set(target.position)) {
                    continue;
                }
                best_ability = &ability;
                best_damage = damage;
                best_landing = landing;
            } catch (const cambc::error&) {
                continue;
            }
        }
        if (best_ability) {
            try {
                ability_used = best_landing ? best_ability->use(*best_landing) : best_ability->use(target.view);

                if (ability_used) {
                    if (best_ability->is_dash()) memory.movement.cooldown = 8;
                    note_damage(memory, target, round);
                }
            } catch (const cambc::error&) {
                // A rejected spell leaves any unused attack or movement available.
            }
        }
    }

    auto attack_tiles = me.attack_tiles();
    if (me.can_move() && !attack_tiles.is_set(target.position)) {
        if (nav::move_towards(memory.movement, target.position, true, false)) {
            attack_tiles = me.attack_tiles();
        }
    }

    if (me.can_attack() && attack_tiles.is_set(target.position)) {
        try {
            me.attack(target.view);
            note_damage(memory, target, round);
            if (!ability_used) {
                auto abilities = me.abilities();
                for (std::size_t slot = 0; slot < abilities.size(); ++slot) {
                    auto& ability = abilities[slot];
                    if (!ability.is_attack_reset() || !ability.is_ready()) continue;
                    try {
                        if (ability.use()) {
                            const auto after_reset_tiles = me.attack_tiles();
                            if (me.can_attack() && after_reset_tiles.is_set(target.view.position())) {
                                me.attack(target.view);
                                note_damage(memory, target, round);
                            }
                        }
                    } catch (const cambc::error&) { }
                    break;
                }
            }
        } catch (const cambc::error&) {
            // The target may have moved or become invalid before the attack.
        }
    }
}
}

bool combat_turn(MatchData& match, BotMemory& memory, float engage_distance,
                 cambc::vec2* target_position, bool keep_mode_while_visible) {
    auto& me = cambc::self();
    const auto round = cambc::round_number();
    const auto my_position = me.position();
    const auto& visible = visible_enemies_for_round(match);

    std::vector<Enemy> enemies;
    enemies.reserve(visible.size());
    for (auto& champion : visible) {
        try {
            if (!champion.is_alive()) continue;
            const auto position = champion.position();
            const auto health = champion.health();
            const auto max_health = champion.max_health();
            const auto* profile = get_champion_profile(champion.champion_name());
            const auto dps = estimated_dps(champion, profile);
            const auto ratio = max_health > 0.0f ? dps / max_health : 0.0f;
            enemies.push_back({champion, static_cast<cambc::handle_champion_t>(champion), position,
                               health, ratio, match.enemy_spawn_avoidance_zone.is_set(position)});
        } catch (const cambc::error&) { }
    }

    Enemy* target = nullptr;
    if (memory.pursuit_target) {
        const auto found = std::find_if(enemies.begin(), enemies.end(), [&](const Enemy& enemy) {
            return enemy.handle == *memory.pursuit_target;
        });
        if (found == enemies.end() || found->health <= 0.0f || found->in_spawn_zone) {
            clear_pursuit(memory);
        } else {
            if (round - memory.pursuit_last_damage_round >= kPursuitLimit) {
                memory.abandoned_pursuit_target = found->handle;
                memory.abandoned_pursuit_last_health = found->health;
                memory.abandoned_pursuit_last_seen_round = round;
                clear_pursuit(memory);
            } else {
                target = &*found;
            }
        }
    }

    if (memory.abandoned_pursuit_target) {
        const auto abandoned = std::find_if(enemies.begin(), enemies.end(), [&](const Enemy& enemy) {
            return enemy.handle == *memory.abandoned_pursuit_target;
        });
        if (abandoned == enemies.end()) {
            clear_abandoned_pursuit(memory);
        } else if (can_damage_target_now(*abandoned, memory, my_position)) {
            clear_abandoned_pursuit(memory);
        } else if (round > 0 && memory.abandoned_pursuit_last_seen_round == round - 1 &&
                   abandoned->health < memory.abandoned_pursuit_last_health) {
            clear_abandoned_pursuit(memory);
        } else {
            memory.abandoned_pursuit_last_health = abandoned->health;
            memory.abandoned_pursuit_last_seen_round = round;
        }
    }

    Enemy* best_nearby = nullptr;
    Enemy* best_spawn_suppressed = nullptr;
    for (auto& enemy : enemies) {
        if (enemy.position.chebyshev_distance(my_position) > engage_distance) continue;
        if (memory.abandoned_pursuit_target && enemy.handle == *memory.abandoned_pursuit_target) continue;
        auto*& best = enemy.in_spawn_zone ? best_spawn_suppressed : best_nearby;
        if (!best || is_better_target(enemy, *best, my_position)) best = &enemy;
    }
    if (target && best_nearby && best_nearby->handle != target->handle &&
        is_better_target(*best_nearby, *target, my_position)) {
        target = best_nearby;
        memory.pursuit_target = target->handle;
        memory.pursuit_last_damage_round = round;
    } else if (!target) {
        target = best_nearby;
        if (!target) {
            if (best_spawn_suppressed) {
                clear_pursuit(memory);
                if (me.is_submitting_gems()) {
                    try { me.cancel_submit_gems(); }
                    catch (const cambc::error&) { return true; }
                }
                retreat_toward_own_spawn(match, memory, my_position, target_position);
                return true;
            }
            if (keep_mode_while_visible && !enemies.empty()) {
                if (target_position) {
                    const auto abandoned = std::find_if(enemies.begin(), enemies.end(), [&](const Enemy& enemy) {
                        return memory.abandoned_pursuit_target && enemy.handle == *memory.abandoned_pursuit_target;
                    });
                    *target_position = abandoned != enemies.end() ? abandoned->position : enemies.front().position;
                }
                return true;
            }
            return false;
        }
        memory.pursuit_target = target->handle;
        memory.pursuit_last_damage_round = round;
    }

    if (target && target->in_spawn_zone) {
        clear_pursuit(memory);
        retreat_toward_own_spawn(match, memory, my_position, target_position);
        return true;
    }

    if (target_position) *target_position = target->position;
    if (me.is_submitting_gems()) {
        try {
            me.cancel_submit_gems();
        } catch (const cambc::error&) {
            return true;
        }
    }

    fight_target(*target, memory, round);
    return true;
}
