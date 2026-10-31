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
constexpr float kEngageDistance = 5.0f;

struct Enemy {
    cambc::champion view;
    cambc::handle_champion_t handle;
    cambc::vec2 position;
    float health;
    float damage_health_ratio;
};

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

void fight_target(Enemy& target, BotMemory& memory, std::uint32_t round) {
    auto& me = cambc::self();
    auto my_position = me.position();
    bool ability_used = false;
    bool moved = false;

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
                const bool damaging = ability.is_damage() || ability.is_aoe_damage() ||
                    (ability.is_cc() && ability.get<cambc::ability_cc>().modifier > 0.0f) ||
                    (ability.is_dash() && ability.get<cambc::ability_dash>().modifier > 0.0f);
                if (!damaging) continue;
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
                    if (best_landing) {
                        moved = true;
                        my_position = me.position();
                    }
                    note_damage(memory, target, round);
                }
            } catch (const cambc::error&) {

                // A rejected spell leaves any unused attack or movement available.
            }
        }
    }

    auto attack_tiles = me.attack_tiles();
    if (me.can_move() && !attack_tiles.is_set(target.position)) {
        moved = nav::move_towards(memory.movement, target.position, true, false);

        if (moved) {
            my_position = me.position();
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
                        const bool reset_used = ability.use();

                        if (reset_used) {
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

bool combat_turn(MatchData& match, BotMemory& memory) {
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
                               health, ratio});
        } catch (const cambc::error&) { }
    }

    Enemy* target = nullptr;
    if (memory.pursuit_target) {
        const auto found = std::find_if(enemies.begin(), enemies.end(), [&](const Enemy& enemy) {
            return enemy.handle == *memory.pursuit_target;
        });
        if (found == enemies.end() || found->health <= 0.0f) {
            clear_pursuit(memory);
        } else {
            if (round - memory.pursuit_last_damage_round >= kPursuitLimit) {
                memory.abandoned_pursuit_target = found->handle;
                clear_pursuit(memory);
            } else {
                target = &*found;
            }
        }
    }

    if (!memory.pursuit_target && memory.abandoned_pursuit_target) {
        const auto abandoned = std::find_if(enemies.begin(), enemies.end(), [&](const Enemy& enemy) {
            return enemy.handle == *memory.abandoned_pursuit_target;
        });
        if (abandoned == enemies.end() || abandoned->position.chebyshev_distance(my_position) > kEngageDistance) {
            memory.abandoned_pursuit_target.reset();
        }
    }

    Enemy* best_nearby = nullptr;
    for (auto& enemy : enemies) {
        if (enemy.position.chebyshev_distance(my_position) > kEngageDistance) continue;
        if (memory.abandoned_pursuit_target && enemy.handle == *memory.abandoned_pursuit_target) continue;
        if (!best_nearby || is_better_target(enemy, *best_nearby, my_position)) best_nearby = &enemy;
    }
    if (target && best_nearby && best_nearby->handle != target->handle &&
        is_better_target(*best_nearby, *target, my_position)) {
        target = best_nearby;
        memory.pursuit_target = target->handle;
        memory.pursuit_last_damage_round = round;
    } else if (!target) {
        target = best_nearby;
        if (!target) {

            return false;
        }
        memory.pursuit_target = target->handle;
        memory.pursuit_last_damage_round = round;
    }

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
