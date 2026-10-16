#include "sdk_adapter.h"
#include <algorithm>
#include <variant>
#include <vector>
#include "design_table.h"
#include "simulator.h"
#include "budget.h"
#include "path_cache.h"
#include <iostream>
#include "../constants/combat_constants.h"
#include "../core/roster.h"

namespace {
namespace bc = cambc;

bool has_durability_emblem(const bc::champion& c) {
    try {
        const auto items = c.item_count();
        return items[bc::item_type::durability_emblem_of_strength] > 0 || items[bc::item_type::durability_emblem_of_focus] > 0
            || items[bc::item_type::durability_emblem_of_health] > 0;
    } catch (const std::exception&) {
        return false;   // not readable for this champion: assume no reduction (worst case for them, see ledger)
    }
}

Fighter fighter_from(const bc::champion& c, bool is_self) {
    Fighter f;
    f.design = &design_for(static_cast<int>(c.champion_name()));
    f.health = c.health();
    f.max_health = c.max_health();
    f.strength = c.strength();
    f.focus = c.focus();
    for (const auto& b : c.buffs()) {
        if (const auto* s = std::get_if<bc::buff_shield>(&b)) { f.shield = s->value; f.shield_turns = s->duration; }
        if (const auto* s = std::get_if<bc::buff_stun>(&b)) f.stunned_rounds = s->duration;
    }
    // Cooldowns: our own are always readable; others through a view built while visible.
    // Anything unreadable counts as ready (worst case).
    try {
        const auto& view = is_self ? c : ability_view(c);
        const auto abilities = view.abilities();
        for (int i = 0; i < 2; ++i) f.cooldown[i] = std::max(0, abilities[i].cooldown());
    } catch (const std::exception&) {
        f.cooldown[0] = f.cooldown[1] = 0;
    }
    if (has_durability_emblem(c)) f.damage_taken_mult = constants::kDurabilityDamageTakenMult;
    return f;
}

float camp_damage_near(bc::vec2 tile) {
    float total = 0;
    for (const auto& camp : bc::vision::camps())
        if (camp.health() < camp.max_health()
            && camp.position().chebyshev_distance(tile) <= constants::kCampAttackRange)
            total += camp.strength();
    return total;
}

std::uint64_t g_setup_fuel = 0, g_sim_fuel = 0;
bool g_path_hit = false;
std::uint64_t fuel_now() { return bc::fuel_remaining(); }

// Position in its team's turn order. The engine orders by priority, then draft order
// (GameRules, "Who goes first?"); our bot never changes priority and enemy priorities are not
// readable out of vision, so draft order (the roster order) is used. Fixed for the match.
int turn_position(const bc::champion& c) {
    const auto& roster = c.team() == bc::team::ally ? roster_allies() : roster_enemies();
    for (std::size_t i = 0; i < roster.size(); ++i)
        if (roster[i] == c) return static_cast<int>(i);
    return 0;
}

struct CacheEntry {
    std::uint32_t round = 0;
    bool used = false, full = false;
    DuelSetup setup;
    int round_cap = 0;
    FightResult result;
};
}  // namespace

DuelSetup duel_setup(const bc::champion& ours, const bc::champion& enemy, const FightOptions& options) {
    DuelSetup setup;
    const auto& me = bc::self();
    setup.us = fighter_from(ours, ours == me);
    setup.enemy = fighter_from(enemy, enemy == me);
    setup.walk_distance = static_cast<int>(ours.position().chebyshev_distance(enemy.position()));
    setup.enemy_acted_this_round = enemy.has_completed_turn();
    setup.same_turn_position = turn_position(ours) == turn_position(enemy);
    setup.our_turn_now = ours == me;   // an ally being predicted for is not the one acting now
    if (options.include_camps) {
        setup.camp_damage_to_us_per_round = camp_damage_near(ours.position());
        setup.camp_damage_to_enemy_per_round = camp_damage_near(enemy.position());
    }
    return setup;
}

FightResult predict_duel(const bc::champion& ours, const bc::champion& enemy, bool full, const FightOptions& options) {
    g_path_hit = false;
    // Live stats need both champions visible and alive; anything else has no answer (and reading a
    // hidden champion throws, which costs a lot of fuel in wasm).
    if (!in_view(ours) || !in_view(enemy)) return FightResult{};
    const auto f0 = bc::fuel_remaining();
    const DuelSetup setup = duel_setup(ours, enemy, options);
    FightOptions opts = options;
    opts.round_cap = std::min(opts.round_cap, constants::kDuelRoundCap);

    // Reuse an answer only for identical inputs in the same round (resolved answers only).
    static CacheEntry cache[8];
    static unsigned next_slot = 0;
    const auto round = bc::round_number();
    for (const auto& e : cache)
        if (e.used && e.round == round && e.full == full && e.round_cap == opts.round_cap && same_inputs(e.setup, setup))
            return e.result;

    // Fast version: if the fight has gone exactly as an earlier prediction said, the current state
    // is a point on its path and that answer still holds (path_cache.h). Only for our own turn
    // order and without camp damage, where recorded points are exact.
    struct PathSlot {
        std::uint32_t round = 0;
        const ChampionDesign* ours = nullptr;    // the matchup this path was calculated for
        const ChampionDesign* enemy = nullptr;
        StoredPath path;
    };
    static PathSlot paths[constants::kPathCacheSlots];
    static unsigned next_path = 0;
    const bool path_ok = constants::kPathCacheEnabled && !full && setup.our_turn_now && !options.include_camps;
    if (path_ok) {
        for (const auto& slot : paths) {
            // Only paths calculated for this matchup can match; skip the rest without scanning.
            if (slot.ours != setup.us.design || slot.enemy != setup.enemy.design) continue;
            if (slot.path.points.empty() || round - slot.round > constants::kPathCacheRounds) continue;
            if (const auto hit = reuse_from_path(slot.path, setup)) {
                g_path_hit = true;
                if constexpr (constants::kVerifyPathCache) {
                    FightOptions check = opts;
                    check.fuel_budget = 0;
                    check.fuel_left = nullptr;
                    const auto fresh = simulate_duel_fast(setup, check);
                    const bool same = fresh.we_win == hit->we_win && fresh.resolved == hit->resolved
                        && fresh.our_health_worst_case == hit->our_health_worst_case
                        && fresh.enemy_health_at_end == hit->enemy_health_at_end && fresh.turns == hit->turns;
                    std::cout << "PATH_CHECK r=" << round << " same=" << same << '\n';
                }
                return *hit;
            }
        }
    }

    // Never spend more simulation fuel than the turn has left after the reserve; a caller's own
    // budget, if given, can only lower it.
    const auto budget = prediction_budget(full, bc::fuel_remaining(), constants::kFastSimFuelBudget, constants::kFullFuelReserve);
    if (budget == 0) return FightResult{};
    opts.fuel_budget = options.fuel_budget > 0 ? std::min(options.fuel_budget, budget) : budget;
    if (!opts.fuel_left) opts.fuel_left = fuel_now;
    static std::vector<PathPoint> recorded;
    recorded.clear();
    if (path_ok) { opts.path_out = &recorded; opts.path_max_points = constants::kPathCachePoints; }
    const auto f1 = bc::fuel_remaining();
    const auto result = full ? simulate_duel(setup, opts) : simulate_duel_fast(setup, opts);
    if (path_ok && result.resolved && !recorded.empty()) {
        auto& slot = paths[next_path];
        slot.round = round;
        slot.ours = setup.us.design;
        slot.enemy = setup.enemy.design;
        slot.path.points.swap(recorded);   // no copy; `recorded` is cleared before its next use
        slot.path.result = result;
        next_path = (next_path + 1) % constants::kPathCacheSlots;
    }
    g_setup_fuel = f0 - f1;
    g_sim_fuel = f1 - bc::fuel_remaining();

    if (result.resolved) {
        cache[next_slot] = CacheEntry{round, true, full, setup, opts.round_cap, result};
        next_slot = (next_slot + 1) % 8u;
    }
    return result;
}

FightResult predict_duel_fast(const bc::champion& enemy, const FightOptions& options) {
    return predict_duel(bc::self(), enemy, false, options);
}
FightResult predict_duel_full(const bc::champion& enemy, const FightOptions& options) {
    return predict_duel(bc::self(), enemy, true, options);
}

bool two_v_one_favourable(const bc::champion& ally_a, const bc::champion& ally_b, const bc::champion& enemy) {
    auto fraction = [](const bc::champion& c) { return c.max_health() > 0 ? c.health() / c.max_health() : 0.0f; };
    if (fraction(ally_a) >= constants::kTwoVOneAllyMinHealth && fraction(ally_b) >= constants::kTwoVOneAllyMinHealth
        && fraction(enemy) <= constants::kTwoVOneEnemyMaxHealth)
        return true;
    // Cassi and Issac die together, so killing the weaker one ends the fight: no shortcut for them.
    const auto a = ally_a.champion_name(), b = ally_b.champion_name();
    const bool linked_pair = (a == bc::champion_type::cassi && b == bc::champion_type::issac)
        || (a == bc::champion_type::issac && b == bc::champion_type::cassi);
    if (linked_pair) return false;
    // Either ally winning alone means the pair wins: the enemy's worst case is to focus one.
    return predict_duel(ally_a, enemy, false, {}).we_win || predict_duel(ally_b, enemy, false, {}).we_win;
}

std::uint64_t last_setup_fuel() { return g_setup_fuel; }
std::uint64_t last_sim_fuel() { return g_sim_fuel; }
bool last_call_hit_path_cache() { return g_path_hit; }
