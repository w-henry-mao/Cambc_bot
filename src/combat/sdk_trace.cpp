#include "sdk_trace.h"
#include <cambc.h>
#include <iostream>
#include <optional>
#include <string>
#include "sdk_adapter.h"
#include "../brain/state.h"
#include "../brain/state_selector.h"
#include "../constants/combat_constants.h"
#include "../core/roster.h"

namespace {
namespace bc = cambc;
constexpr int kContactDistance = 4;     // a fight starts when an enemy is this close
constexpr int kSeparateDistance = 9;    // ...and ends unresolved when it is farther for kSeparateTurns
constexpr int kSeparateTurns = 3;
constexpr int kCrowdDistance = 4;       // a third champion this close makes the fight unclean

struct Fight {
    std::optional<bc::champion> target;
    int enemy_type = -1;
    std::uint32_t start_round = 0;
    float start_hp = 0, enemy_start_hp = 0;
    FightResult fast, full;
    bool clean = true, policy_ok = true;
    int far_turns = 0;
    float last_enemy_hp = 0;
};
std::optional<Fight> fight;
bool fight_finished = false;

std::string name(int type) { return bc::to_string(static_cast<bc::champion_type>(type)); }

void print_result(const char* tag, const FightResult& r, std::uint64_t fuel) {
    std::cout << ' ' << tag << "_win=" << r.we_win << ' ' << tag << "_hp=" << r.our_health_worst_case << ' ' << tag
              << "_enemy_hp=" << r.enemy_health_at_end << ' ' << tag << "_turns=" << r.turns << ' ' << tag
              << "_resolved=" << r.resolved << ' ' << tag << "_fuel=" << fuel;
}

std::optional<bc::champion> nearest_enemy(const bc::champion& me) {
    std::optional<bc::champion> best;
    for (const auto& e : bc::vision::champions(bc::team::enemy))
        if (in_view(e) && (!best || e.position().chebyshev_distance(me.position()) < best->position().chebyshev_distance(me.position())))
            best = e;
    return best;
}

bool crowded(const bc::champion& me, const bc::champion& enemy) {
    for (const auto& c : bc::vision::champions()) {
        if (c == me || c == enemy || !in_view(c)) continue;
        if (c.position().chebyshev_distance(me.position()) <= kCrowdDistance
            || c.position().chebyshev_distance(enemy.position()) <= kCrowdDistance) return true;
    }
    for (const auto& camp : bc::vision::camps())
        if (camp.health() < camp.max_health()
            && (camp.position().chebyshev_distance(me.position()) <= constants::kCampAttackRange
                || camp.position().chebyshev_distance(enemy.position()) <= constants::kCampAttackRange)) return true;
    return false;
}

void end_fight(const char* how, float our_hp, float enemy_hp) {
    std::cout << "FIGHT_END r=" << bc::round_number() << " me=" << bc::to_string(bc::self().champion_name())
              << " enemy=" << name(fight->enemy_type) << " start_round=" << fight->start_round << " how=" << how
              << " our_hp=" << our_hp << " enemy_hp=" << enemy_hp << " clean=" << fight->clean
              << " policy_ok=" << fight->policy_ok << '\n';
    fight.reset();
    fight_finished = true;
}

void print_fighter(const char* tag, const Fighter& f) {
    std::cout << ' ' << tag << "=" << f.design->type << ',' << f.health << ',' << f.max_health << ',' << f.strength << ','
              << f.focus << ',' << f.shield << ',' << f.shield_turns << ',' << f.cooldown[0] << ',' << f.cooldown[1] << ','
              << f.stunned_rounds << ',' << f.damage_taken_mult;
}
void print_setup(const DuelSetup& d) {
    print_fighter("S_us", d.us);
    print_fighter("S_en", d.enemy);
    std::cout << " S_d=" << d.walk_distance << " S_acted=" << d.enemy_acted_this_round << " S_same=" << d.same_turn_position;
}
}  // namespace

void combat_trace_turn_start() {
    if constexpr (!constants::kCombatTrace) return;
    else {
        try {
            auto& me = bc::self();
            if (fight || !me.is_alive()) return;
            const auto enemy = nearest_enemy(me);
            if (!enemy || enemy->position().chebyshev_distance(me.position()) > kContactDistance) return;
            if (bc::fuel_remaining() < 60000) return;   // keep trace builds from running out mid-turn
            Fight f;
            f.target = *enemy;
            f.enemy_type = static_cast<int>(enemy->champion_name());
            const auto dist = enemy->position().chebyshev_distance(me.position());
            f.start_round = bc::round_number();
            f.start_hp = me.health();
            f.enemy_start_hp = f.last_enemy_hp = enemy->health();
            f.clean = !crowded(me, *enemy);
            const auto t0 = bc::fuel_remaining();
            f.fast = predict_duel_fast(*enemy);
            const auto t1 = bc::fuel_remaining();
            const auto fast_setup = last_setup_fuel(), fast_sim = last_sim_fuel();
            // A real caller uses the full version on spare-fuel turns, not after the fast one on a
            // busy turn; skip it when fuel is short so the trace does not cause mid-turn pauses.
            if (bc::fuel_remaining() > 70000) f.full = predict_duel_full(*enemy);
            const auto t2 = bc::fuel_remaining();
            std::cout << "FIGHT_PRED r=" << f.start_round << " me=" << bc::to_string(me.champion_name())
                      << " enemy=" << name(f.enemy_type) << " dist=" << dist << " our_hp=" << f.start_hp
                      << " enemy_hp=" << f.enemy_start_hp << " clean=" << f.clean << " fast_setup_fuel=" << fast_setup
                      << " fast_sim_fuel=" << fast_sim << " full_sim_fuel=" << last_sim_fuel();
            print_setup(duel_setup(me, *enemy));
            print_result("fast", f.fast, t0 - t1);
            print_result("full", f.full, t1 - t2);
            std::cout << '\n';
            fight = f;
        } catch (const std::exception& e) {
            std::cout << "FIGHT_TRACE_ERROR " << e.what() << '\n';
        }
    }
}

void combat_trace_turn() {
    if constexpr (!constants::kCombatTrace) return;
    else {
        try {
            if (!fight) return;
            auto& me = bc::self();
            if (!me.is_alive()) { end_fight("we_died", 0, fight->last_enemy_hp); return; }
            const auto& target = fight->target;
            bool target_alive = true;
            try { target_alive = target && target->is_alive(); } catch (const std::exception&) {}
            if (!target_alive) { end_fight("enemy_died", me.health(), 0); return; }
            const bool visible = target && in_view(*target);
            if (visible) {
                fight->last_enemy_hp = target->health();
                if (crowded(me, *target)) fight->clean = false;
                if (me.is_recalling() || me.is_submitting_gems()) fight->policy_ok = false;
                // Whenever the selector ran this turn, the bot must have been in its attack state
                // (another state, e.g. farming, moves us in ways the fight model does not).
                const auto st = last_selected_state();
                if (st != State::Attack && st != State::None) fight->policy_ok = false;
            }
            if (visible) {
                const auto d = duel_setup(me, *target);
                std::cout << "FIGHT_TURN r=" << bc::round_number() << " me=" << bc::to_string(me.champion_name());
                print_setup(d);
                std::cout << '\n';
            }
            const bool far = !visible || target->position().chebyshev_distance(me.position()) > kSeparateDistance;
            fight->far_turns = far ? fight->far_turns + 1 : 0;
            if (fight->far_turns >= kSeparateTurns) end_fight("separated", me.health(), fight->last_enemy_hp);
        } catch (const std::exception& e) {
            std::cout << "FIGHT_TRACE_ERROR " << e.what() << '\n';
        }
    }
}

bool combat_trace_fight_finished() { return fight_finished; }
