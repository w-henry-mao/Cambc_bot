#include <cambc.h>
#include <array>
#include <cstddef>
#include <span>
#include "farm.h"
#include "../comms/shared_memory.h"
#include "../comms/team_comms.h"
#include "../comms/ally_positions.h"
#include "../constants/combat_constants.h"
#include "../core/context.h"
#include "../core/geometry.h"
#include "../core/match_data.h"
#include "../core/roster.h"
#include "../nav/blocked_tiles.h"
#include "../nav/pathfinding.h"
#include "../skills/casting.h"
#include "../constants/brain_constants.h"
#include "../constants/state_constants.h"
namespace {
constexpr int kUnreached = 65535;

float discount(int rounds) {
    static const auto table = [] {
        std::array<float, 512> powers{};
        powers[0] = 1.0f;
        for (std::size_t i = 1; i < powers.size(); ++i) powers[i] = powers[i - 1] * constants::kRoundDiscount;
        return powers;
    }();
    return table[static_cast<std::size_t>(std::clamp(rounds, 0, static_cast<int>(table.size()) - 1))];
}

// Damage per round of a team of `count` champions like us.
struct Team { float dps = 0; int count = 0; };
float my_dps() { auto& me = cambc::self(); return me.strength() + constants::kAbilityDpsPerFocus * me.focus(); }

// value = (gems + gold x weight) x discount ^ (walk + kill + capture); nullopt if not worth it.
std::optional<float> camp_value(const CachedCampGroup& group, int walk, const Team& team, float my_health,
                                float max_damage_share = constants::kMaxCampDamageShare) {
    const auto round = cambc::round_number();
    if (group.respawn_ready_round > round + static_cast<std::uint32_t>(walk)) return std::nullopt;   // still respawning on arrival
    int kill = 0;
    if (!group.reward_waiting) {
        if (team.dps <= 0) return std::nullopt;
        kill = static_cast<int>(group.design_health / team.dps) + 1;
        // Camps hit one champion at a time; skip camps that would hurt us too much.
        if (group.design_strength * static_cast<float>(kill) / static_cast<float>(team.count) > max_damage_share * my_health)
            return std::nullopt;
    }
    const float reward = static_cast<float>(group.gem_value) + constants::kGoldWeight * static_cast<float>(group.gold_value);
    return reward * discount(walk + kill + cambc::constants::capture_time);
}

struct Pick { std::optional<int> group; float value = 0; };
// Best camp for `team` by walking distance from us; keeps `current` unless another is clearly better.
// Whether `group` passes the danger limit for `team` (as in camp_value, ignoring respawn timers).
bool safe_enough(const CachedCampGroup& group, const Team& team, float my_health, float max_damage_share) {
    if (team.dps <= 0) return false;
    const int kill = static_cast<int>(group.design_health / team.dps) + 1;
    return group.design_strength * static_cast<float>(kill) / static_cast<float>(team.count) <= max_damage_share * my_health;
}

// Best camp for `team` by walking distance from us; keeps `current` unless another is clearly better.
// `starved` (gem_starved: no gem group one champion could take, e.g. the only gems are in monoliths): gem groups are taken with kMaxCampDamageShareGemStarved instead, since
// otherwise we would never bank.
// Map-level: no gem group passes the danger limit for `team` (see farm_target).
bool gem_starved(const MatchData& match, const Team& team, float my_health) {
    for (const auto& group : match.camp_groups)
        if (group.gem_value > 0 && safe_enough(group, team, my_health, constants::kMaxCampDamageShare)) return false;
    return true;
}

Pick best_camp(const MatchData& match, const cambc::distance_field& field, const Team& team, std::optional<int> current,
               bool starved, std::optional<int> exclude = std::nullopt) {
    const float health = cambc::self().health();
    const bool gem_group_ok = !starved;
    Pick best, kept;
    for (int i = 0; i < static_cast<int>(match.camp_groups.size()); ++i) {
        if (exclude == i) continue;
        const auto& group = match.camp_groups[static_cast<std::size_t>(i)];
        const int walk = field.get(group.centre);
        if (walk == kUnreached) continue;
        const float share = !gem_group_ok && group.gem_value > 0 ? constants::kMaxCampDamageShareGemStarved : constants::kMaxCampDamageShare;
        const auto value = camp_value(group, walk, team, health, share);
        if (!value) continue;
        if (current == i) kept = {i, *value};
        if (!best.group || *value > best.value) best = {i, *value};
    }
    if (kept.group && best.value < constants::kSwitchFactor * kept.value) return kept;
    return best;
}

int alive_allies() {
    int n = 0;
    for (const auto& ally : roster_allies()) n += ally.is_alive();
    return n;
}

// The team target lives in the shared header word (comms/shared_memory.h: camp group + 1 and the
// round written, mod 8). The first living champion in draft order picks it each round, judging
// camps with the whole team's damage; the others read it.
bool i_lead() {
    for (const auto& ally : roster_allies())
        if (ally.is_alive()) return ally == cambc::self();
    return false;
}
void write_team_target(std::optional<int> group) {
    write_team_header(team_comms::set_farm(read_team_header(), group ? *group + 1 : 0, cambc::round_number()));
}
std::optional<int> read_team_target(const MatchData& match) {
    const auto farm = team_comms::get_farm(read_team_header());
    const int group = farm.group_plus1 - 1;
    const int age = team_comms::age(cambc::round_number(), farm.round_low, team_comms::kFarmRoundBits);
    if (group < 0 || group >= static_cast<int>(match.camp_groups.size()) || age > team_comms::kFarmTargetRounds) return std::nullopt;
    return group;
}
}

// Two groups (constants::kSplitGroups), by roster position (draft order: Ren, Stellar, Cassi,
// Issac, Ryth, Tavi): Ren and Cassi & Issac (linked, so together) in group 0, the rest in 1.
// Positions, not champion_name(): reading another champion's type can throw.
int group_of_index(int i) { return i == 0 || i == 2 || i == 3 ? 0 : 1; }
int my_roster_index() {
    const auto& allies = roster_allies();
    for (int i = 0; i < static_cast<int>(allies.size()); ++i)
        if (allies[static_cast<std::size_t>(i)] == cambc::self()) return i;
    return 0;
}
// Alive members of each group.
std::array<int, 2> group_sizes() {
    std::array<int, 2> n{0, 0};
    const auto& allies = roster_allies();
    for (int i = 0; i < static_cast<int>(allies.size()); ++i)
        if (allies[static_cast<std::size_t>(i)].is_alive()) ++n[static_cast<std::size_t>(group_of_index(i))];
    return n;
}
bool i_lead_group(int g) {
    const auto& allies = roster_allies();
    for (int i = 0; i < static_cast<int>(allies.size()); ++i)
        if (allies[static_cast<std::size_t>(i)].is_alive() && group_of_index(i) == g) return allies[static_cast<std::size_t>(i)] == cambc::self();
    return false;
}

// Per-champion choice (constants::kPerChampionCamps): our own best group, counting teammates
// near each group as helpers plus a bonus each. Keeps `current` unless another is clearly better.
std::optional<int> own_best_camp(const MatchData& match, const cambc::distance_field& field, float dps, std::optional<int> current) {
    auto& me = cambc::self();
    // Teammates' positions: exact while visible, else their last position report.
    cambc::vec2 mates[8];
    int n_mates = 0;
    const auto& allies = roster_allies();
    const auto round = cambc::round_number();
    for (int i = 0; i < static_cast<int>(allies.size()) && n_mates < 8; ++i) {
        const auto& a = allies[static_cast<std::size_t>(i)];
        if (a == me) continue;
        if (in_view(a)) { mates[n_mates++] = a.position(); continue; }
        const auto& report = ally_position(i);
        if (report.known && round - report.heard_round <= 2u * static_cast<std::uint32_t>(constants::kAllyPositionMaxAge)) mates[n_mates++] = report.pos;
    }
    const float health = me.health();
    const std::size_t width = static_cast<std::size_t>(match.map_width);
    Pick best, kept;
    for (int i = 0; i < static_cast<int>(match.camp_groups.size()); ++i) {
        const auto& group = match.camp_groups[static_cast<std::size_t>(i)];
        const int walk = field.get(group.centre);
        if (walk == kUnreached) continue;
        int helpers = 0;
        for (int k = 0; k < n_mates; ++k) helpers += static_cast<int>(mates[k].chebyshev_distance(group.centre)) <= constants::kHelpRadius;
        const std::size_t idx = static_cast<std::size_t>(group.centre.y) * width + static_cast<std::size_t>(group.centre.x);
        const bool near_spawn = width > 0 && idx < match.home_walk.size() && match.home_walk[idx] <= constants::kNearSpawnWalk;
        const auto value = camp_value(group, walk, Team{dps * static_cast<float>(1 + helpers), 1 + helpers}, health,
                                      near_spawn ? constants::kMaxCampDamageShareNearSpawn : constants::kMaxCampDamageShare);
        if (!value) continue;
        const float v = *value + constants::kCampTeammateBonus * static_cast<float>(helpers);
        if (current == i) kept = {i, v};
        if (!best.group || v > best.value) best = {i, v};
    }
    if (kept.group && best.value < constants::kSwitchFactor * kept.value) return kept.group;
    return best.group;
}

// Once per round: our farm target. Go solo when a camp we can take alone is worth at least as
// much as the team target; otherwise join the team target.
std::optional<int> farm_target(MatchData& match) {
    static std::uint32_t round = 0;
    if (round == cambc::round_number()) return match.farm_target;
    round = cambc::round_number();
    auto& me = cambc::self();
    cambc::distance_field field;
    cambc::bitboard visited;
    cambc::bitboard camp_tiles;
    camp_tiles.clear();
    for (const auto& g : match.camp_groups) camp_tiles |= g.spawn_tiles;
    // Camps stand on their own tiles: make every group's tiles walkable for the estimate.
    field.compute(match.static_blockers & ~camp_tiles, tiles_around(me.position(), 0), visited, constants::kCampSearchRange);
    const float dps = my_dps();
    const int team_size = std::max(1, alive_allies());
    // A property of the map: no gem group even one champion at full health could take within the
    // normal limit (normal maps have wolves, elks or gem clusters; stampede and contest only monoliths).
    const bool starved = gem_starved(match, Team{dps, 1}, me.max_health());
    std::optional<int> team_target;
    int target_size = team_size;   // champions sharing the target we follow
    std::array<int, 2> sizes{0, 0};
    if constexpr (constants::kSplitGroups) sizes = group_sizes();
    const bool split = constants::kSplitGroups && sizes[0] >= 2 && sizes[1] >= 2;
    if (constants::kPerChampionCamps) {
        // No shared team target: every champion decides for itself (opening first, below).
    } else if (split) {
        // Two groups: group 0 uses the header's farm target, group 1 bytes 6-7 and avoids group 0's.
        const int g = group_of_index(my_roster_index());
        target_size = sizes[static_cast<std::size_t>(g)];
        if (g == 0) {
            if (i_lead_group(0)) {
                team_target = best_camp(match, field, Team{dps * static_cast<float>(target_size), target_size}, match.farm_target, starved).group;
                write_team_target(team_target);
            } else {
                team_target = read_team_target(match);
            }
        } else if (i_lead_group(1)) {
            team_target = best_camp(match, field, Team{dps * static_cast<float>(target_size), target_size}, match.farm_target, starved,
                                    read_team_target(match)).group;
            write_group2_target(team_target ? *team_target + 1 : 0);
        } else if (const int g2 = read_group2_target(team_comms::kFarmTargetRounds); g2 > 0 && g2 <= static_cast<int>(match.camp_groups.size())) {
            team_target = g2 - 1;
        }
    } else if (i_lead()) {
        team_target = best_camp(match, field, Team{dps * static_cast<float>(team_size), team_size}, match.farm_target, starved).group;
        write_team_target(team_target);
    } else {
        team_target = read_team_target(match);
    }
    if constexpr (constants::kOpeningClosestCamp) {
        // Opening: our own closest camp group (by walk from where we first look), until it is
        // cleared and starts respawning, or kOpeningMaxRounds pass.
        static std::optional<int> opening;
        static bool opening_done = false;
        if (!opening_done) {
            if (!opening) {
                int best_walk = kUnreached;
                for (int i = 0; i < static_cast<int>(match.camp_groups.size()); ++i) {
                    const int walk = field.get(match.camp_groups[static_cast<std::size_t>(i)].centre);
                    if (walk < best_walk) { best_walk = walk; opening = i; }
                }
            }
            const bool cleared = opening && match.camp_groups[static_cast<std::size_t>(*opening)].respawn_ready_round > round;
            if (!opening || cleared || round > constants::kOpeningMaxRounds) {
                opening_done = true;
            } else {
                match.farm_target = opening;
                return match.farm_target;
            }
        }
    }
    if constexpr (constants::kPerChampionCamps) {
        match.farm_target = own_best_camp(match, field, dps, match.farm_target);
        return match.farm_target;
    }
    const auto solo = best_camp(match, field, Team{dps, 1}, match.farm_target, starved);
    float team_value = 0;
    if (team_target) {
        const auto& group = match.camp_groups[static_cast<std::size_t>(*team_target)];
        const int walk = field.get(group.centre);
        if (walk != kUnreached)
            if (const auto v = camp_value(group, walk, Team{dps * static_cast<float>(target_size), target_size}, me.health(),
                                          starved && group.gem_value > 0 ? constants::kMaxCampDamageShareGemStarved : constants::kMaxCampDamageShare))
                team_value = *v;
    }
    // A team target we would not take (respawning by the time we arrive, too dangerous) is ignored.
    if (team_value <= 0) team_target.reset();
    if constexpr (constants::kStayTogether) {
        // Stay together: always the team target (solo only when there is none).
        match.farm_target = team_target ? team_target : solo.group;
        return match.farm_target;
    }
    match.farm_target = (solo.group && solo.value >= team_value) ? solo.group : team_target;
    return match.farm_target;
}

int FarmState::score(const TurnContext& ctx) { return constants::kFarmScore; }

namespace {
// Stay together (constants::kStayTogether): where the group leader (first living champion in
// draft order) is, seen or from its last position report; nullopt if we lead or do not know.
std::optional<cambc::vec2> group_anchor() {
    const auto& allies = roster_allies();
    for (int i = 0; i < static_cast<int>(allies.size()); ++i) {
        const auto& a = allies[static_cast<std::size_t>(i)];
        if (!a.is_alive()) continue;
        if (a == cambc::self()) return std::nullopt;
        if (in_view(a)) return a.position();
        const auto& report = ally_position(i);
        if (report.known && cambc::round_number() - report.heard_round <= 2u * static_cast<std::uint32_t>(constants::kAllyPositionMaxAge))
            return report.pos;
        return std::nullopt;
    }
    return std::nullopt;
}
}

StateResult FarmState::execute(TurnContext& ctx, BotMemory& memory) {
    auto& me = cambc::self();
    auto& match = *ctx.match_data;
    if (!farm_target(match)) return StateResult::Failed;
    if constexpr (constants::kBunchMode == 1) {
        // Rally: a teammate saw enemies nearby; join it before farming on.
        const auto rally = read_rally(constants::kRallyRounds);
        if (rally.set && me.can_move()) {
            const cambc::vec2 spot{rally.x, rally.y};
            const int d = distance(me.position(), spot);
            if (d <= constants::kRallyRange && d > constants::kRallyRadius) {
                move_towards_closest(tiles_around(spot, constants::kRallyRadius));
                return StateResult::Continue;
            }
        }
    }
    if constexpr (constants::kBunchMode == 2) {
        // Local: enemies in sight and our nearest visible teammate is away: close up with it.
        bool enemy_near = false;
        for (const auto& e : roster_enemies())
            if (in_view(e) && distance(e.position(), me.position()) <= constants::kRallyTrigger) { enemy_near = true; break; }
        if (enemy_near && me.can_move()) {
            std::optional<cambc::vec2> mate;
            int best = 1 << 20;
            for (const auto& a : roster_allies()) {
                if (a == me || !in_view(a)) continue;
                const int d = distance(a.position(), me.position());
                if (d < best) { best = d; mate = a.position(); }
            }
            if (mate && best > constants::kRallyRadius) {
                move_towards_closest(tiles_around(*mate, constants::kRallyRadius));
                return StateResult::Continue;
            }
        }
    }
    if constexpr (constants::kStayTogether) {
        // Drifted away from the leader: walk back before farming on.
        if (const auto anchor = group_anchor(); anchor && me.can_move() && distance(me.position(), *anchor) > constants::kGroupRadius) {
            move_towards_closest(tiles_around(*anchor, constants::kGroupRadius / 2));
            return StateResult::Continue;
        }
    }
    const auto& group = match.camp_groups[static_cast<std::size_t>(*match.farm_target)];
    // Hit the weakest live camp of our target in range: ability first, then basic attack.
    const cambc::camp* weakest = nullptr;
    for (const auto& camp : *group.view)
        if (camp.is_visible() && camp.health() > 0 && me.attack_tiles().is_set(camp.position()) && (!weakest || camp.health() < weakest->health()))
            weakest = &camp;
    if (weakest) {
        cast_at(*weakest);
        attack_with_reset(*weakest);
        return StateResult::Continue;
    }
    if (group.spawn_tiles.is_set(me.position())) {
        if (group.reward_waiting) return StateResult::Continue;   // capturing
        // Captured already: an occupied spawn tile stops the whole group respawning, so step off.
        if (me.can_move()) {
            const auto blocked = blocked_tiles();
            for (const auto tile : blocked.reachable_tiles(me.position(), me.base_stats().base_move_speed).positions())
                if (!group.spawn_tiles.is_set(tile) && me.move_absolute(tile)) break;
        }
        return StateResult::Continue;
    }
    move_towards_closest(group.spawn_tiles);
    return StateResult::Continue;
}
FarmState farm_state_instance;
