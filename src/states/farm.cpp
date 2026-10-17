#include <cambc.h>
#include <array>
#include <cstddef>
#include <span>
#include "farm.h"
#include "../comms/shared_memory.h"
#include "../comms/team_comms.h"
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
std::optional<float> camp_value(const CachedCampGroup& group, int walk, const Team& team, float my_health) {
    const auto round = cambc::round_number();
    if (group.respawn_ready_round > round + static_cast<std::uint32_t>(walk)) return std::nullopt;   // still respawning on arrival
    int kill = 0;
    if (!group.reward_waiting) {
        if (team.dps <= 0) return std::nullopt;
        kill = static_cast<int>(group.design_health / team.dps) + 1;
        // Camps hit one champion at a time; skip camps that would hurt us too much.
        if (group.design_strength * static_cast<float>(kill) / static_cast<float>(team.count) > constants::kMaxCampDamageShare * my_health)
            return std::nullopt;
    }
    const float reward = static_cast<float>(group.gem_value) + constants::kGoldWeight * static_cast<float>(group.gold_value);
    return reward * discount(walk + kill + cambc::constants::capture_time);
}

struct Pick { std::optional<int> group; float value = 0; };
// Best camp for `team` by walking distance from us; keeps `current` unless another is clearly better.
Pick best_camp(const MatchData& match, const cambc::distance_field& field, const Team& team, std::optional<int> current) {
    const float health = cambc::self().health();
    Pick best, kept;
    for (int i = 0; i < static_cast<int>(match.camp_groups.size()); ++i) {
        const auto& group = match.camp_groups[static_cast<std::size_t>(i)];
        const int walk = field.get(group.centre);
        if (walk == kUnreached) continue;
        const auto value = camp_value(group, walk, team, health);
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
    std::optional<int> team_target;
    if (i_lead()) {
        team_target = best_camp(match, field, Team{dps * static_cast<float>(team_size), team_size}, match.farm_target).group;
        write_team_target(team_target);
    } else {
        team_target = read_team_target(match);
    }
    const auto solo = best_camp(match, field, Team{dps, 1}, match.farm_target);
    float team_value = 0;
    if (team_target) {
        const auto& group = match.camp_groups[static_cast<std::size_t>(*team_target)];
        const int walk = field.get(group.centre);
        if (walk != kUnreached)
            if (const auto v = camp_value(group, walk, Team{dps * static_cast<float>(team_size), team_size}, me.health())) team_value = *v;
    }
    // A team target we would not take (respawning by the time we arrive, too dangerous) is ignored.
    if (team_value <= 0) team_target.reset();
    match.farm_target = (solo.group && solo.value >= team_value) ? solo.group : team_target;
    return match.farm_target;
}

int FarmState::score(const TurnContext& ctx) { return constants::kFarmScore; }

StateResult FarmState::execute(TurnContext& ctx, BotMemory& memory) {
    auto& me = cambc::self();
    auto& match = *ctx.match_data;
    if (!farm_target(match)) return StateResult::Failed;
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
