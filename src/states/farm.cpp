#include <cambc.h>
#include <array>
#include <cstddef>
#include <span>
#include <tuple>
#include <vector>
#include "farm.h"
#include "shop.h"
#include "../comms/shared_memory.h"
#include "../comms/team_comms.h"
#include "../comms/ally_positions.h"
#include "../constants/combat_constants.h"
#include "../core/context.h"
#include "../core/geometry.h"
#include "../core/match_data.h"
#include "../core/group_routes.h"
#include "../core/roster.h"
#include "../init/deferred_setup.h"
#include "../nav/blocked_tiles.h"
#include "../nav/pathfinding.h"
#include "../skills/casting.h"
#include "../constants/brain_constants.h"
#include "../constants/state_constants.h"
#include "../constants/test_hooks.h"
namespace {
constexpr int kUnreached = 65535;

// Monolith-only maps (gem_starved, set each round by farm_target) may use their own discount
// (constants::kStarvedRoundDiscount): their gems sit behind long kills.
bool g_starved_map = false;
float discount(int rounds) {
    static const auto table = [] {
        std::array<float, 512> powers{};
        powers[0] = 1.0f;
        for (std::size_t i = 1; i < powers.size(); ++i) powers[i] = powers[i - 1] * constants::kRoundDiscount;
        return powers;
    }();
    if constexpr (constants::kStarvedRoundDiscount != constants::kRoundDiscount) {
        static const auto starved_table = [] {
            std::array<float, 512> powers{};
            powers[0] = 1.0f;
            for (std::size_t i = 1; i < powers.size(); ++i) powers[i] = powers[i - 1] * constants::kStarvedRoundDiscount;
            return powers;
        }();
        if (g_starved_map) return starved_table[static_cast<std::size_t>(std::clamp(rounds, 0, static_cast<int>(starved_table.size()) - 1))];
    }
    return table[static_cast<std::size_t>(std::clamp(rounds, 0, static_cast<int>(table.size()) - 1))];
}

// Damage per round of a team of `count` champions like us.
struct Team { float dps = 0; int count = 0; };
float my_dps() { auto& me = cambc::self(); return me.strength() + constants::kAbilityDpsPerFocus * me.focus(); }

// Gold's weight against gems in camp values. Default: constants::kGoldWeight all game. Test plans
// (constants::kGoldDecayPlan) value early gold more, since it buys damage for the rest of the game:
// 1: (winning gems - enemy gems banked) / kGoldDecayScale; 2: kGoldDecayStart x (1 - round /
// kGoldDecayRounds); 3: kGoldDecayStart until our planned emblems are bought. None goes below
// kGoldWeight.
float gold_weight() {
    if constexpr (constants::kGoldDecayPlan == 1) {
        // One engine query per round (this runs for every camp group scored).
        static int cached_round = -1;
        static float cached = constants::kGoldWeight;
        if (cambc::round_number() != cached_round) {
            cached_round = cambc::round_number();
            const float left = static_cast<float>(cambc::constants::winning_gems) - static_cast<float>(cambc::global_submitted_gems(cambc::team::enemy));
            cached = std::max(constants::kGoldWeight, left / constants::kGoldDecayScale);
        }
        return cached;
    } else if constexpr (constants::kGoldDecayPlan == 3) {
        // Until our planned emblems are finished (states/shop.h), then the normal weight. Item
        // counts are read once per round.
        static int cached_round = -1;
        static float cached = constants::kGoldWeight;
        if (cambc::round_number() != cached_round) {
            cached_round = cambc::round_number();
            cached = next_emblem_gold(cambc::self()) ? constants::kGoldDecayStart : constants::kGoldWeight;
        }
        return cached;
    } else if constexpr (constants::kGoldDecayPlan == 2) {
        const float frac = 1.0f - static_cast<float>(cambc::round_number()) / constants::kGoldDecayRounds;
        return std::max(constants::kGoldWeight, constants::kGoldDecayStart * frac);
    }
    return constants::kGoldWeight;
}

// value = (gems + gold x weight) x discount ^ (walk + kill + capture); nullopt if not worth it.
std::optional<float> camp_value(const CachedCampGroup& group, int walk, const Team& team, float my_health,
                                float max_damage_share = constants::kMaxCampDamageShare) {
    const auto round = cambc::round_number();
    if (respawn_ready(group) > round + static_cast<std::uint32_t>(walk)) return std::nullopt;   // still respawning on arrival
    // Gold-only groups on the enemy's side (constants::kEnemySidePlan): little to gain, and the
    // enemy fights there next to its spawn. 1: closer to their spawn than ours; 2: within
    // kEnemySideRadius of their spawn.
    if constexpr (constants::kEnemySidePlan > 0) {
        if (group.gem_value == 0) {
            if (constants::kEnemySidePlan == 1 && group.enemy_spawn_dist < group.own_spawn_dist) return std::nullopt;
            if (constants::kEnemySidePlan == 2 && group.enemy_spawn_dist <= constants::kEnemySideRadius) return std::nullopt;
        }
    }
    int kill = 0;
    // A reward we left to a teammate was collected by now, and the camps are back.
    if (!group.reward_waiting || group.left_to_teammate_round) {
        if (team.dps <= 0) return std::nullopt;
        kill = static_cast<int>(group.design_health / team.dps) + 1;
        // Camps hit one champion at a time; skip camps that would hurt us too much.
        const float taken = constants::kSeqCampDamage ? group.seq_damage_unit / team.dps / static_cast<float>(team.count)
                                                      : group.design_strength * static_cast<float>(kill) / static_cast<float>(team.count);
        if (taken > max_damage_share * my_health) return std::nullopt;
    }
    const float reward = static_cast<float>(group.gem_value) + gold_weight() * static_cast<float>(group.gold_value);
    return reward * discount(walk + kill + cambc::constants::capture_time);
}

struct Pick { std::optional<int> group; float value = 0; };
// Route lookahead (constants::kRouteLookahead): what the best camp after group `from` adds, for
// a team that finishes `from` in `finish` rounds: walking distance between groups from the draft's
// table, the next group must be up by the time we get there, and the same danger limit applies.
float next_camp_bonus(const MatchData& match, int from, int finish, const Team& team, float my_health, float max_damage_share,
                      int depth = constants::kRouteDepth) {
    if (!match.routes.ready() || team.dps <= 0 || depth <= 0) return 0;
    const auto round = cambc::round_number();
    float best = 0;
    struct Next { int group = -1; float value = 0; int finish = 0; };
    std::array<Next, constants::kRouteBranch> top{};
    for (int j = 0; j < static_cast<int>(match.camp_groups.size()); ++j) {
        if (j == from) continue;
        const int d = match.routes.get(from, j);
        if (d == group_routes::kFar) continue;
        const auto& group = match.camp_groups[static_cast<std::size_t>(j)];
        const int arrive = finish + d;
        if (respawn_ready(group) > round + static_cast<std::uint32_t>(arrive)) continue;
        const int kill = static_cast<int>(group.design_health / team.dps) + 1;
        if (group.design_strength * static_cast<float>(kill) / static_cast<float>(team.count) > max_damage_share * my_health) continue;
        const float reward = static_cast<float>(group.gem_value) + gold_weight() * static_cast<float>(group.gold_value);
        const float value = reward * discount(arrive + kill + cambc::constants::capture_time);
        if (depth == 1) { best = std::max(best, value); continue; }
        // Deeper routes (constants::kRouteDepth > 1): the best kRouteBranch next camps by their own
        // value also count the camp after them.
        const Next next{j, value, arrive + kill + cambc::constants::capture_time};
        for (int k = 0; k < constants::kRouteBranch; ++k)
            if (top[static_cast<std::size_t>(k)].group < 0 || next.value > top[static_cast<std::size_t>(k)].value) {
                for (int m = constants::kRouteBranch - 1; m > k; --m) top[static_cast<std::size_t>(m)] = top[static_cast<std::size_t>(m - 1)];
                top[static_cast<std::size_t>(k)] = next;
                break;
            }
    }
    if (depth > 1)
        for (const auto& t : top)
            if (t.group >= 0)
                best = std::max(best, t.value + next_camp_bonus(match, t.group, t.finish, team, my_health, max_damage_share, depth - 1));
    return best;
}
// Rounds until a team finishes `group` from `walk` steps away (walk + kill + capture).
int finish_rounds(const CachedCampGroup& group, int walk, const Team& team) {
    const int kill = (!group.reward_waiting || group.left_to_teammate_round) && team.dps > 0
        ? static_cast<int>(group.design_health / team.dps) + 1 : 0;
    return walk + kill + cambc::constants::capture_time;
}

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
               bool starved, std::optional<int> exclude = std::nullopt, float normal_share = constants::kMaxCampDamageShare,
               const cambc::vec2* mates = nullptr, int n_mates = 0) {
    const float health = cambc::self().health();
    const bool gem_group_ok = !starved;
    Pick best, kept;
    struct Candidate { int group = 0; float value = 0; int walk = 0; float share = 0; };
    static std::array<Candidate, 64> candidates;
    int n_candidates = 0;
    for (int i = 0; i < static_cast<int>(match.camp_groups.size()); ++i) {
        if (exclude == i) continue;
        const auto& group = match.camp_groups[static_cast<std::size_t>(i)];
        const int walk = field.get(group.centre);
        if (walk == kUnreached) continue;
        const float share = !gem_group_ok && group.gem_value > 0 ? constants::kMaxCampDamageShareGemStarved : normal_share;
        std::optional<float> value;
        if (mates) {
            // Teammates near the camp help: more damage, damage taken shared.
            int helpers = 0;
            for (int k = 0; k < n_mates; ++k) helpers += static_cast<int>(mates[k].chebyshev_distance(group.centre)) <= constants::kHelpRadius;
            const Team with_help{team.dps * static_cast<float>(team.count + helpers) / static_cast<float>(team.count), team.count + helpers};
            value = camp_value(group, walk, with_help, health, share);
        } else {
            value = camp_value(group, walk, team, health, share);
        }
        if (!value) continue;
        if constexpr (constants::kRouteLookahead > 0) {
            if (n_candidates < static_cast<int>(candidates.size())) candidates[static_cast<std::size_t>(n_candidates++)] = {i, *value, walk, share};
            continue;
        }
        if (current == i) kept = {i, *value};
        if (!best.group || *value > best.value) best = {i, *value};
    }
    if constexpr (constants::kRouteLookahead > 0) {
        // The best few by their own value (and the current target) also count the camp after them.
        std::sort(candidates.begin(), candidates.begin() + n_candidates,
                  [](const Candidate& a, const Candidate& b) { return a.value > b.value; });
        for (int k = 0; k < n_candidates; ++k) {
            auto& c = candidates[static_cast<std::size_t>(k)];
            if (k >= constants::kRouteLookahead && current != c.group) continue;
            const auto& group = match.camp_groups[static_cast<std::size_t>(c.group)];
            c.value += next_camp_bonus(match, c.group, finish_rounds(group, c.walk, team), team, health, c.share);
            if (current == c.group) kept = {c.group, c.value};
            if (!best.group || c.value > best.value) best = {c.group, c.value};
        }
    }
    const float factor = constants::kCommitTarget > 0 ? std::max(constants::kSwitchFactor, constants::kCommitTarget) : constants::kSwitchFactor;
    if (kept.group && best.value < factor * kept.value) return kept;
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

// Teammates' positions: exact while visible, else their last position report (at most 8).
int collect_mates(cambc::vec2 (&mates)[8]) {
    auto& me = cambc::self();
    int n = 0;
    const auto& allies = roster_allies();
    const auto round = cambc::round_number();
    for (int i = 0; i < static_cast<int>(allies.size()) && n < 8; ++i) {
        const auto& a = allies[static_cast<std::size_t>(i)];
        if (a == me || !a.is_alive()) continue;
        if (in_view(a)) { mates[n++] = a.position(); continue; }
        const auto& report = ally_position(i);
        if (report.known && round - report.heard_round <= 2u * static_cast<std::uint32_t>(constants::kAllyPositionMaxAge)) mates[n++] = report.pos;
    }
    return n;
}

// Capture stayer (constants::kCaptureStayer): whether we are the one to capture `group`, chosen
// among us and visible allies near it (ties: earlier roster position).
bool i_am_stayer(const CachedCampGroup& group) {
    auto& me = cambc::self();
    const auto radius = static_cast<std::uint32_t>(constants::kHelpRadius + 2);
    auto key = [&](const cambc::champion& c) -> float {
        switch (constants::kCaptureStayer) {
            case 1: return static_cast<float>(c.position().chebyshev_distance(group.centre));
            case 2: return c.health() / c.max_health();
            default: return c.strength() + constants::kAbilityDpsPerFocus * c.focus();
        }
    };
    const float mine = key(me);
    const auto& allies = roster_allies();
    for (int i = 0; i < static_cast<int>(allies.size()); ++i) {
        const auto& a = allies[static_cast<std::size_t>(i)];
        if (a == me) break;   // allies after us lose ties
        if (!in_view(a) || a.position().chebyshev_distance(group.centre) > radius) continue;
        if (key(a) <= mine) return false;
    }
    for (std::size_t i = 0; i < allies.size(); ++i) {
        const auto& a = allies[i];
        if (a == me || !in_view(a) || a.position().chebyshev_distance(group.centre) > radius) continue;
        if (key(a) < mine) return false;
    }
    return true;
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
    Pick best, kept;
    for (int i = 0; i < static_cast<int>(match.camp_groups.size()); ++i) {
        const auto& group = match.camp_groups[static_cast<std::size_t>(i)];
        const int walk = field.get(group.centre);
        if (walk == kUnreached) continue;
        int helpers = 0;
        for (int k = 0; k < n_mates; ++k) helpers += static_cast<int>(mates[k].chebyshev_distance(group.centre)) <= constants::kHelpRadius;
        const bool near_spawn = match.home_ready && match.home_distance.get(group.centre) <= constants::kNearSpawnWalk;
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
std::optional<int> farm_target(MatchData& match, bool force) {
    static std::uint32_t round = 0;
    if (!force && round == cambc::round_number()) return match.farm_target;
    round = cambc::round_number();
    auto& me = cambc::self();
    // Walking distances from us, kept for the round: a forced re-pick later in the same turn
    // (leaving a reward to a teammate, before we move) re-scores the camps without a new search.
    static cambc::distance_field field;
    static std::uint32_t field_round = 0;
    if (field_round != round) {
        field_round = round;
        cambc::bitboard visited;
        cambc::bitboard camp_tiles;
        camp_tiles.clear();
        for (const auto& g : match.camp_groups) camp_tiles |= g.spawn_tiles;
        // Camps stand on their own tiles: make every group's tiles walkable for the estimate.
        field.compute(match.static_blockers & ~camp_tiles, tiles_around(me.position(), 0), visited, constants::kCampSearchRange);
    }
    const float dps = my_dps();
    const int team_size = std::max(1, alive_allies());
    // A property of the map: no gem group even one champion at full health could take within the
    // normal limit (normal maps have wolves, elks or gem clusters; stampede and contest only monoliths).
    const bool starved = gem_starved(match, Team{dps, 1}, me.max_health());
    g_starved_map = starved;
    // Capture stayer: a cleared group of ours waiting for capture is kept only by its stayer;
    // everyone else (and the team target) moves on.
    std::optional<int> capture_exclude;
    if constexpr (constants::kCaptureStayer > 0) {
        if (const auto cur = match.farm_target) {
            const auto& g = match.camp_groups[static_cast<std::size_t>(*cur)];
            if (g.reward_waiting) {
                if (i_am_stayer(g) && g.centre.chebyshev_distance(me.position()) <= static_cast<std::uint32_t>(constants::kHelpRadius + 2))
                    return match.farm_target;
                capture_exclude = cur;
            }
        }
    }
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
                static std::optional<int> last_group_target;
                const auto current = constants::kCommitTarget > 0 ? last_group_target : match.farm_target;
                team_target = best_camp(match, field, Team{dps * static_cast<float>(target_size), target_size}, current, starved).group;
                last_group_target = team_target;
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
        // kCommitTarget: the team target keeps itself (the leader's own pick may be a solo camp).
        static std::optional<int> last_team_target;
        const auto current = constants::kCommitTarget > 0 ? last_team_target : match.farm_target;
        team_target = best_camp(match, field, Team{dps * static_cast<float>(team_size), team_size}, current, starved,
                                capture_exclude).group;
        last_team_target = team_target;
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
            const bool cleared = opening && respawn_ready(match.camp_groups[static_cast<std::size_t>(*opening)]) > round;
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
    cambc::vec2 mates[8];
    const int n_mates = constants::kRealBackup ? collect_mates(mates) : 0;
    if (capture_exclude && team_target == capture_exclude) team_target.reset();
    const auto solo = best_camp(match, field, Team{dps, 1}, match.farm_target, starved, capture_exclude, constants::kSoloDamageShare,
                                constants::kRealBackup ? mates : nullptr, n_mates);
    float team_value = 0;
    if (team_target) {
        const auto& group = match.camp_groups[static_cast<std::size_t>(*team_target)];
        const int walk = field.get(group.centre);
        if (walk != kUnreached)
            if (const auto v = camp_value(group, walk, Team{dps * static_cast<float>(target_size), target_size}, me.health(),
                                          starved && group.gem_value > 0 ? constants::kMaxCampDamageShareGemStarved : constants::kMaxCampDamageShare)) {
                team_value = *v;
                if constexpr (constants::kRouteLookahead > 0) {
                    const Team team{dps * static_cast<float>(target_size), target_size};
                    team_value += next_camp_bonus(match, *team_target, finish_rounds(group, walk, team), team, me.health(),
                                                  starved && group.gem_value > 0 ? constants::kMaxCampDamageShareGemStarved : constants::kMaxCampDamageShare);
                }
            }
    }
    // A team target we would not take (respawning by the time we arrive, too dangerous) is ignored.
    if (team_value <= 0) team_target.reset();
    if constexpr (constants::kStayTogether) {
        // Stay together: always the team target (solo only when there is none).
        match.farm_target = team_target ? team_target : solo.group;
        return match.farm_target;
    }
    const auto previous = match.farm_target;
    match.farm_target = (solo.group && solo.value >= team_value) ? solo.group : team_target;
    // Commit to the camp we are walking to: switch only if it is no longer worth taking (respawning
    // by the time we arrive, too dangerous) or the new pick is worth kCommitTarget times as much.
    if constexpr (constants::kCommitTarget > 0) {
        if (previous && match.farm_target && *previous != *match.farm_target) {
            const auto value_of = [&](int i) {
                const auto& group = match.camp_groups[static_cast<std::size_t>(i)];
                const int walk = field.get(group.centre);
                if (walk == kUnreached) return 0.0f;
                const bool team = team_target && *team_target == i;
                const float share = starved && group.gem_value > 0 ? constants::kMaxCampDamageShareGemStarved
                                    : team ? constants::kMaxCampDamageShare : constants::kSoloDamageShare;
                const Team t = team ? Team{dps * static_cast<float>(target_size), target_size} : Team{dps, 1};
                return camp_value(group, walk, t, me.health(), share).value_or(0.0f);
            };
            const float kept = value_of(*previous);
            if (kept > 0 && value_of(*match.farm_target) < constants::kCommitTarget * kept) match.farm_target = previous;
        }
    }
    // Enough of us at the team target already for a camp this strong: farm elsewhere.
    if constexpr (constants::kCampCrowdRounds > 0) {
        if (match.farm_target && match.farm_target == team_target && dps > 0) {
            const auto& g = match.camp_groups[static_cast<std::size_t>(*team_target)];
            const auto r = static_cast<std::uint32_t>(constants::kHelpRadius);
            if (g.centre.chebyshev_distance(me.position()) > r) {
                const int needed = std::max(1, static_cast<int>(g.design_health / (dps * static_cast<float>(constants::kCampCrowdRounds)) + 0.999f));
                cambc::vec2 crowd[8];
                const int n = collect_mates(crowd);
                int there = 0;
                for (int k = 0; k < n; ++k) there += crowd[k].chebyshev_distance(g.centre) <= r;
                if (there >= needed) {
                    const auto other = best_camp(match, field, Team{dps, 1}, std::nullopt, starved, team_target);
                    if (other.group) match.farm_target = other.group;
                }
            }
        }
    }
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

// Items as shards: a shard counts 1 and an emblem kEmblemShards (emblems cannot be bought). The
// weight of each item kind is looked up once; nullopt if the items cannot be read.
std::optional<int> item_shards(const cambc::champion& c) {
    try {
        const auto counts = c.item_count();
        static std::vector<int> weights;
        if (weights.empty())
            for (std::uint32_t i = 0; i < counts.size(); ++i)
                weights.push_back(cambc::item_type(i).is_purchasable() ? 1 : constants::kEmblemShards);
        int shards = 0;
        std::size_t i = 0;
        for (const auto n : counts) shards += static_cast<int>(n) * weights[i++];
        return shards;
    } catch (const cambc::error&) {
        return std::nullopt;
    }
}

int distance_to_group(const CachedCampGroup& group, cambc::vec2 position) {
    int best = 1 << 20;
    for (const auto tile : group.spawn_positions) best = std::min(best, distance(tile, position));
    return best;
}

// Single collector (constants::kSingleCollector): whether a teammate rather than us should
// collect `group`'s reward and already stands on one of its spawn tiles. Every champion ranks the
// visible champions within kCollectorRadius of the group the same way: fewest items, then
// closest, then roster order.
bool teammate_collects(const CachedCampGroup& group) {
    auto& me = cambc::self();
    const auto& allies = roster_allies();
    const cambc::champion* collector = nullptr;
    std::tuple<int, int, int, int> best{};
    for (int i = 0; i < static_cast<int>(allies.size()); ++i) {
        const auto& a = allies[static_cast<std::size_t>(i)];
        if (!(a == me) && !in_view(a)) continue;
        if (a.is_recalling() || a.is_submitting_gems() || a.is_teleporting()) continue;
        const int d = distance_to_group(group, a.position());
        if (d > constants::kCollectorRadius) continue;
        const auto shards = item_shards(a);
        if (!shards) continue;
        // constants::kCollectorPreferRanged: ranged champions first (they stand back while the camps
        // die, so the melee ones were usually on the spawn tiles when the capture finished).
        const int melee = constants::kCollectorPreferRanged && a.attack_range() <= 1 ? 1 : 0;
        const std::tuple<int, int, int, int> key{melee, *shards, d, i};
        if (!collector || key < best) { collector = &a; best = key; }
    }
    return collector && !(*collector == me) && group.spawn_tiles.is_set(collector->position());
}

// Step onto a reachable tile outside the group's spawn tiles.
void step_off(const CachedCampGroup& group) {
    auto& me = cambc::self();
    if (!me.can_move()) return;
    const auto blocked = blocked_tiles();
    for (const auto tile : blocked.reachable_tiles(me.position(), me.base_stats().base_move_speed).positions())
        if (!group.spawn_tiles.is_set(tile) && me.move_absolute(tile)) break;
}
}

StateResult FarmState::execute(TurnContext& ctx, BotMemory& memory) {
    g_farm_action = 'E';
    auto& me = cambc::self();
    auto& match = *ctx.match_data;
    if (!farm_target(match)) return StateResult::Failed;
    if constexpr (constants::kEarlyRetreat) {
        // Enemies at our camp outnumber us there (counting us as on our way): back off home.
        const auto& g = match.camp_groups[static_cast<std::size_t>(*match.farm_target)];
        const auto r = static_cast<std::uint32_t>(constants::kEarlyRetreatRadius);
        int enemies = 0;
        for (const auto& e : roster_enemies())
            if (in_view(e) && e.position().chebyshev_distance(g.centre) <= r) ++enemies;
        if (enemies > 0) {
            cambc::vec2 mates[8];
            const int n = collect_mates(mates);
            int ours = 1;
            for (int k = 0; k < n; ++k) ours += mates[k].chebyshev_distance(g.centre) <= r;
            if (enemies > ours && me.can_move()) {
                move_towards_closest(cambc::map::spawn_region(cambc::team::ally));
                return StateResult::Continue;
            }
        }
    }
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
    if constexpr (constants::kSingleCollector) {
        // A teammate collects this reward: leave it the spawn tiles and farm something else.
        auto& target = match.camp_groups[static_cast<std::size_t>(*match.farm_target)];
        if (target.reward_waiting && !target.left_to_teammate_round && teammate_collects(target)) {
            target.left_to_teammate_round = cambc::round_number();
            if (target.spawn_tiles.is_set(me.position())) {
                step_off(target);
                return StateResult::Continue;
            }
            if (!farm_target(match, true)) return StateResult::Failed;
        }
    }
    const auto& group = match.camp_groups[static_cast<std::size_t>(*match.farm_target)];
    // Hit the weakest live camp of our target in range: ability first, then basic attack.
    const cambc::camp* weakest = nullptr;
    for (const auto& camp : *group.view)
        if (camp.is_visible() && camp.health() > 0 && me.attack_tiles().is_set(camp.position()) && (!weakest || camp.health() < weakest->health()))
            weakest = &camp;
    if (weakest) {
        const bool cast = cast_at(*weakest);
        const bool hit = attack_with_reset(*weakest);
        g_farm_action = cast || hit ? 'H' : 'h';
        return StateResult::Continue;
    }
    if (group.spawn_tiles.is_set(me.position())) {
        if (group.reward_waiting) { g_farm_action = 'C'; return StateResult::Continue; }   // capturing
        // Captured already: an occupied spawn tile stops the whole group respawning, so step off.
        g_farm_action = 'S';
        step_off(group);
        return StateResult::Continue;
    }
    g_farm_action = respawn_ready(group) > static_cast<std::uint32_t>(cambc::round_number()) && !group.reward_waiting ? 'W' : group.reward_waiting ? 'C' : 'A';
    if constexpr (STATE_TRACE) {
        g_farm_detail[0] = g_farm_detail[2] = -1;
        g_farm_detail[1] = me.can_move();
        int live = 0;
        for (const auto& camp : *group.view)
            if (camp.is_visible() && camp.health() > 0) {
                ++live;
                const int d = static_cast<int>(camp.position().chebyshev_distance(me.position()));
                if (g_farm_detail[0] < 0 || d < g_farm_detail[0]) g_farm_detail[0] = d;
            }
        g_farm_detail[2] = live;
    }
    // Reach a live camp (constants::kReachCamp): walk to the closest free tile from which one is
    // in attack range, instead of onto the spawn tiles (most of which the camps stand on; melee
    // champions arrived next to the group but out of reach a quarter of the time).
    if constexpr (constants::kReachCamp) {
        if (!group.reward_waiting) {
            cambc::bitboard stand;
            stand.clear();
            cambc::bitboard camps;
            camps.clear();
            for (const auto& camp : *group.view)
                if (camp.is_visible() && camp.health() > 0) {
                    stand.draw_chebyshev(camp.position(), static_cast<float>(me.attack_range()));
                    camps.set(camp.position());
                }
            stand &= ~match.static_blockers & ~camps;
            if (!stand.is_empty()) {
                move_towards_closest(stand);
                return StateResult::Continue;
            }
        }
    }
    // While the group respawns, wait next to its spawn tiles: standing on one would hold the respawn.
    // Otherwise walk onto them (camps of a live group may only be in reach from the spawn tiles of
    // camps already killed; a waiting reward is captured there).
    const auto& beside = constants::kWaitBesideSpawn && respawn_ready(group) != 0 && !group.reward_waiting
        ? beside_tiles(match, match.camp_groups[static_cast<std::size_t>(*match.farm_target)]) : group.beside_tiles;
    if (constants::kWaitBesideSpawn && respawn_ready(group) != 0 && !group.reward_waiting && !beside.is_empty())
        move_towards_closest(beside);
    else
        move_towards_closest(group.spawn_tiles);
    return StateResult::Continue;
}
FarmState farm_state_instance;
char g_farm_action = '-';
int g_farm_detail[3] = {-1, -1, -1};
