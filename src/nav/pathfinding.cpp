#include "pathfinding.h"
#include "../core/rng.h"
#include "../constants/tunable_constants.h"
#include "../constants/game_rules.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace nav {
namespace {
constexpr int kDashRange = game_rules::kDashRange;
constexpr int kDashCooldown = game_rules::kAbilityCooldownRounds;

struct DashOption {
    std::int8_t dx;
    std::int8_t dy;
    std::uint8_t intermediate_count;
    std::array<std::int8_t, 3> intermediate_x;
    std::array<std::int8_t, 3> intermediate_y;
};

constexpr int abs_int(int value) { return value < 0 ? -value : value; }

constexpr std::array<DashOption, 16> make_dash_options() {
    std::array<DashOption, 16> options{};
    std::size_t count = 0;
    for (int dx = -kDashRange; dx <= kDashRange; ++dx) {
        for (int dy = -kDashRange; dy <= kDashRange; ++dy) {
            if (std::max(abs_int(dx), abs_int(dy)) != kDashRange) continue;
            DashOption option{};
            option.dx = static_cast<std::int8_t>(dx);
            option.dy = static_cast<std::int8_t>(dy);
            for (int mx = -1; mx <= 1; ++mx) {
                for (int my = -1; my <= 1; ++my) {
                    if (mx == 0 && my == 0) continue;
                    if (std::max(abs_int(dx - mx), abs_int(dy - my)) <= 1) {
                        const auto index = option.intermediate_count++;
                        option.intermediate_x[index] = static_cast<std::int8_t>(mx);
                        option.intermediate_y[index] = static_cast<std::int8_t>(my);
                    }
                }
            }
            options[count++] = option;
        }
    }
    return options;
}

constexpr auto kDashOptions = make_dash_options();

void refresh_champion_tiles_impl(cambc::bitboard& ally_tiles, cambc::bitboard& enemy_tiles) {
    cambc::vision::champion_tiles_into(cambc::team::ally, ally_tiles);
    cambc::vision::champion_tiles_into(cambc::team::enemy, enemy_tiles);
}

struct DashCandidate {
    int progress;
    cambc::vec2 landing;
};

void refresh_landing_blocked(PathScratch& scratch, cambc::vec2 position,
                             const cambc::bitboard* additional_forbidden_tiles = nullptr) {
    auto& landing_blocked = scratch.landing_blocked;
    landing_blocked.clear();
    landing_blocked |= cambc::map::walls();
    landing_blocked |= cambc::map::holes();
    landing_blocked |= cambc::map::altars();
    landing_blocked |= cambc::map::spawn_region(cambc::team::enemy);
    refresh_champion_tiles_impl(scratch.ally_tiles, scratch.enemy_tiles);
    cambc::vision::camp_tiles_into(scratch.camp_tiles);
    cambc::vision::teleport_tiles_into(scratch.teleport_tiles);
    landing_blocked |= scratch.ally_tiles;
    landing_blocked |= scratch.enemy_tiles;
    landing_blocked |= scratch.camp_tiles;
    landing_blocked |= scratch.teleport_tiles;
    if (additional_forbidden_tiles) landing_blocked |= *additional_forbidden_tiles;
    landing_blocked.set(position, false);
}

bool newly_revealed_bush_tile(const cambc::bitboard& visible_before_move) {
    const auto newly_revealed = cambc::vision::visible_tiles() & ~visible_before_move &
        cambc::map::bushes();
    return !newly_revealed.is_empty();
}

bool path_is_clear(cambc::vec2 start, const DashOption& option, const cambc::bitboard& walls) {
    for (std::size_t i = 0; i < option.intermediate_count; ++i) {
        const cambc::vec2 intermediate{start.x + option.intermediate_x[i],
                                       start.y + option.intermediate_y[i]};
        if (cambc::map::is_in_bounds(intermediate) && !walls.is_set(intermediate)) return true;
    }
    return false;
}
}

void filter_legal_dash_landings(MovementMemory& movement, cambc::vec2 start,
                                const cambc::bitboard& in_range, cambc::bitboard& legal) {
    legal.clear();
    if (!movement.has_dash) {
        return;
    }
    auto& scratch = movement.scratch;
    refresh_landing_blocked(scratch, start);
    const auto& walls = cambc::map::walls();
    for (const auto& landing : in_range.positions()) {
        if (!cambc::map::is_in_bounds(landing) || landing == start ||
            landing.chebyshev_distance(start) > kDashRange || scratch.landing_blocked.is_set(landing)) continue;
        const auto dx = landing.x - start.x;
        const auto dy = landing.y - start.y;
        if (std::max(abs_int(dx), abs_int(dy)) == 1) {
            legal.set(landing);
            continue;
        }
        for (const auto& option : kDashOptions) {
            if (option.dx == dx && option.dy == dy && path_is_clear(start, option, walls)) {
                legal.set(landing);
                break;
            }
        }
    }
}

namespace {
bool walk_towards(MovementMemory& movement, cambc::vec2 target,
                  const cambc::bitboard* additional_forbidden_tiles = nullptr,
                  bool* failed_after_bush_reveal = nullptr) {
    auto& me = cambc::self();
    if (!me.can_move()) return false;
    if (movement.movement_guard_center || additional_forbidden_tiles) {
        const auto position = me.position();
        auto& scratch = movement.scratch;
        refresh_landing_blocked(scratch, position, additional_forbidden_tiles);
        scratch.route_blocked = scratch.landing_blocked;
        scratch.route_blocked.set(target, false);
        scratch.goals.clear();
        scratch.goals.set(target);
        scratch.visited.clear();
        scratch.goal_field.compute(scratch.route_blocked, scratch.goals, scratch.visited);
        const auto step = scratch.goal_field.next_move_from(position);
        if (!step) return false;
        if (movement.movement_guard_center &&
            (position + *step).chebyshev_distance(*movement.movement_guard_center) >
                static_cast<std::uint32_t>(movement.movement_guard_radius)) return false;
        try {
            me.move(*step);
            return true;
        } catch (const cambc::error&) {
            return false;
        }
    }
    try {
        const auto visible_before_move = cambc::vision::visible_tiles();
        if (me.move_towards(target)) return true;
        if (me.can_move() && newly_revealed_bush_tile(visible_before_move)) {
            if (failed_after_bush_reveal) *failed_after_bush_reveal = true;
            return me.move_towards(target);
        }
        return false;
    } catch (const cambc::error&) {
        return false;
    }
}
}

bool move_towards(MovementMemory& movement, cambc::vec2 target, bool allow_dash,
                  bool target_is_camp, const cambc::bitboard* additional_forbidden_tiles,
                  bool* failed_after_bush_reveal) {
    if (failed_after_bush_reveal) *failed_after_bush_reveal = false;
    auto& me = cambc::self();
    if (!me.can_move()) {
        return false;
    }

    const auto position = me.position();
    if (!cambc::map::is_in_bounds(target)) {
        return false;
    }
    if (additional_forbidden_tiles && additional_forbidden_tiles->is_set(target)) return false;
    const auto direct_distance = position.chebyshev_distance(target);
    if (movement.is_heart_of_ruin && target_is_camp &&
        direct_distance < tunable_constants::kHeartOfRuinMinCampDistance) {
        const bool moved = walk_towards(movement, target, additional_forbidden_tiles, failed_after_bush_reveal);
        return moved;
    }

    bool can_dash = movement.has_dash && allow_dash && movement.cooldown == 0 && me.can_cast();
    cambc::ability* ability = nullptr;
    if (can_dash) {
        ability = &*movement.ability;
        if (!ability->is_ready()) {
            movement.cooldown = std::max(1, ability->cooldown());
            can_dash = false;
        }
    }
    if (!can_dash) {
        const bool moved = walk_towards(movement, target, additional_forbidden_tiles, failed_after_bush_reveal);
        return moved;
    }

    auto& scratch = movement.scratch;
    auto& landing_blocked = scratch.landing_blocked;
    refresh_landing_blocked(scratch, position, additional_forbidden_tiles);
    auto& route_blocked = scratch.route_blocked;
    route_blocked = landing_blocked;
    auto& goals = scratch.goals;
    goals.clear();
    goals.set(target);
    route_blocked.set(target, false);

    auto& goal_field = scratch.goal_field;
    scratch.visited.clear();
    goal_field.compute(route_blocked, goals, scratch.visited);
    const int distance = goal_field.get(position);
    if (distance == cambc::distance_field::unreachable) {
        const bool moved = walk_towards(movement, target, additional_forbidden_tiles, failed_after_bush_reveal);
        return moved;
    }

    const int minimum_distance = movement.is_heart_of_ruin && target_is_camp
        ? tunable_constants::kHeartOfRuinMinCampDistance : tunable_constants::kDashMinimumProgress;
    if (distance < minimum_distance) {
        const bool moved = walk_towards(movement, target, additional_forbidden_tiles, failed_after_bush_reveal);
        return moved;
    }

    std::array<DashCandidate, kDashOptions.size()> candidates{};
    std::size_t candidate_count = 0;
    std::size_t best_count = 0;
    const auto& walls = cambc::map::walls();
    for (const auto& option : kDashOptions) {
        const cambc::vec2 landing{position.x + option.dx, position.y + option.dy};
        if (!cambc::map::is_in_bounds(landing)) continue;
        if (movement.movement_guard_center &&
            landing.chebyshev_distance(*movement.movement_guard_center) >
                static_cast<std::uint32_t>(movement.movement_guard_radius)) continue;
        const auto landing_distance = goal_field.get(landing);
        if (landing_distance == cambc::distance_field::unreachable) continue;
        const int progress = distance - std::max(static_cast<int>(landing_distance) - 1, 0);
        if (progress < tunable_constants::kDashMinimumProgress || landing_blocked.is_set(landing)) continue;

        if (path_is_clear(position, option, walls)) candidates[candidate_count++] = {progress, landing};
    }

    best_count = std::min<std::size_t>(3, candidate_count);
    std::partial_sort(candidates.begin(), candidates.begin() + best_count,
                      candidates.begin() + candidate_count,
                      [](const DashCandidate& a, const DashCandidate& b) {
                          return a.progress > b.progress;
                      });

    bool dashed = false;
    for (std::size_t i = 0; i < best_count; ++i) {
        try {
            if (ability->use(candidates[i].landing)) {
                movement.cooldown = kDashCooldown;
                dashed = true;
                break;
            }
        } catch (const cambc::error&) {
            // A failed destination does not consume the cast; try the next-ranked landing.
        } catch (...) {
            // Preserve walking if the SDK reports an unexpected destination failure.
        }
    }
    if (!dashed) {
        const bool moved = walk_towards(movement, target, additional_forbidden_tiles, failed_after_bush_reveal);
        return moved;
    }

    const auto current_position = me.position();
    if (const auto step = goal_field.next_move_from(current_position)) {
        const auto next = current_position + *step;
        const bool within_guard = !movement.movement_guard_center ||
            next.chebyshev_distance(*movement.movement_guard_center) <=
                static_cast<std::uint32_t>(movement.movement_guard_radius);
        if (current_position + *step != target && within_guard && me.can_move()) me.move(*step);
    }
    return true;
}

bool move_into_radius(MovementMemory& movement, cambc::vec2 center, int radius,
                      bool fallback_to_closest,
                      const cambc::bitboard* additional_forbidden_tiles) {
    auto& me = cambc::self();
    const auto position = me.position();
    if (!me.can_move()) return false;
    if (position.chebyshev_distance(center) <= static_cast<std::uint32_t>(radius)) return true;

    auto& scratch = movement.scratch;
    scratch.route_blocked = cambc::map::walls() | cambc::map::holes() | cambc::map::altars() |
        cambc::map::spawn_region(cambc::team::enemy);
    refresh_champion_tiles_impl(scratch.ally_tiles, scratch.enemy_tiles);
    cambc::vision::camp_tiles_into(scratch.camp_tiles);
    cambc::vision::teleport_tiles_into(scratch.teleport_tiles);
    scratch.route_blocked |= scratch.ally_tiles;
    scratch.route_blocked |= scratch.enemy_tiles;
    scratch.route_blocked |= scratch.camp_tiles;
    scratch.route_blocked |= scratch.teleport_tiles;
    if (additional_forbidden_tiles) scratch.route_blocked |= *additional_forbidden_tiles;
    scratch.route_blocked.set(position, false);

    scratch.goals.clear();
    scratch.goals.draw_chebyshev(center, static_cast<float>(radius));
    scratch.goals &= ~scratch.route_blocked;

    cambc::bitboard roots;
    roots.set(position);
    scratch.visited.clear();
    scratch.goal_field.compute(scratch.route_blocked, roots, scratch.visited);
    std::optional<cambc::vec2> destination;
    auto shortest = cambc::distance_field::unreachable;
    for (const auto tile : scratch.goals.positions()) {
        const auto distance = scratch.goal_field.get(tile);
        if (distance < shortest) {
            shortest = distance;
            destination = tile;
        }
    }
    if (!destination && fallback_to_closest) {
        for (int search_radius = radius + 1; search_radius < 100; ++search_radius) {
            std::optional<cambc::vec2> ring_destination;
            auto ring_shortest = cambc::distance_field::unreachable;
            const auto consider = [&](cambc::vec2 tile) {
                if (!cambc::map::is_in_bounds(tile)) return;
                const auto distance = scratch.goal_field.get(tile);
                if (distance != cambc::distance_field::unreachable && distance < ring_shortest) {
                    ring_destination = tile;
                    ring_shortest = distance;
                }
            };

            for (int offset = -search_radius; offset <= search_radius; ++offset) {
                consider({center.x + offset, center.y - search_radius});
                consider({center.x + offset, center.y + search_radius});
            }
            for (int offset = -search_radius + 1; offset < search_radius; ++offset) {
                consider({center.x - search_radius, center.y + offset});
                consider({center.x + search_radius, center.y + offset});
            }
            if (ring_destination) {
                destination = ring_destination;
                break;
            }
        }
    }
    if (!destination) return false;
    return move_towards(movement, *destination, true, false, additional_forbidden_tiles);
}

bool move_randomly_within_radius(MovementMemory& movement, cambc::vec2 center, int radius,
                                 const cambc::bitboard& forbidden_tiles) {
    auto& me = cambc::self();
    if (!me.can_move()) return false;
    const auto position = me.position();
    auto& scratch = movement.scratch;
    refresh_landing_blocked(scratch, position, &forbidden_tiles);

    std::array<cambc::vec2, 8> destinations{};
    std::size_t count = 0;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            const cambc::vec2 destination{position.x + dx, position.y + dy};
            if (!cambc::map::is_in_bounds(destination) ||
                destination.chebyshev_distance(center) > static_cast<std::uint32_t>(radius) ||
                scratch.landing_blocked.is_set(destination)) continue;
            destinations[count++] = destination;
        }
    }
    if (count == 0) return false;

    const auto first = random_number(static_cast<unsigned>(count));
    for (std::size_t i = 0; i < count; ++i) {
        const auto destination = destinations[(first + i) % count];
        try {
            if (me.move({destination.x - position.x, destination.y - position.y})) return true;
        } catch (const cambc::error&) { }
    }
    return false;
}

bool move_off(MovementMemory& movement, const cambc::bitboard& forbidden_tiles) {
    auto& me = cambc::self();
    if (!me.can_move()) return false;
    const auto position = me.position();
    refresh_landing_blocked(movement.scratch, position);
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            const cambc::vec2 destination{position.x + dx, position.y + dy};
            if (!cambc::map::is_in_bounds(destination) || forbidden_tiles.is_set(destination) ||
                movement.scratch.landing_blocked.is_set(destination)) continue;
            if (movement.movement_guard_center &&
                destination.chebyshev_distance(*movement.movement_guard_center) >
                    static_cast<std::uint32_t>(movement.movement_guard_radius)) continue;
            try {
                if (me.move({dx, dy})) return true;
            } catch (const cambc::error&) { }
        }
    }
    return false;
}

void refresh_visible_champion_tiles(cambc::bitboard& ally_tiles, cambc::bitboard& enemy_tiles) {
    refresh_champion_tiles_impl(ally_tiles, enemy_tiles);
}
}
