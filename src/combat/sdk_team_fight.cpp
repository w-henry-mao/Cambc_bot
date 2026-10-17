#include "sdk_team_fight.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <span>
#include <vector>
#include "sdk_adapter.h"
#include "../comms/ally_positions.h"
#include "../constants/combat_constants.h"
#include "../core/roster.h"

namespace {
namespace bc = cambc;

int roster_index(const std::vector<bc::champion>& roster, const bc::champion& c) {
    for (std::size_t i = 0; i < roster.size(); ++i)
        if (roster[i] == c) return static_cast<int>(i);
    return -1;
}
struct Seen { const bc::champion* c; bc::vec2 pos; };
TeamFightOptions quick_options() {
    TeamFightOptions o;
    o.support_weight = constants::kTeamFightSupportWeight;
    return o;
}
}  // namespace

namespace {
// What each teammate looked like when we last saw it (for estimates while it is out of sight).
struct AllyMemory { Fighter f; std::uint32_t round = 0; bool seen = false; };
AllyMemory ally_memory[8];

// A reported teammate as a fighter: the stats we last saw it with, cooldowns run down since,
// health from the report's half-health bit unless what we saw still fits it. Only teammates we
// have seen at least once (reading a hidden champion's type throws, which is costly in wasm;
// teammates spawn together, so each is seen early).
Fighter shared_fighter(const AllyMemory& mem, const AllyPosition& report, std::uint32_t round) {
    Fighter f = mem.f;
    const int since = static_cast<int>(round - mem.round);
    for (int& cd : f.cooldown) cd = std::max(0, cd - since);
    f.shield = 0;
    f.shield_turns = 0;
    f.stunned_rounds = 0;
    const float half = 0.5f * f.max_health;
    const bool fits = report.healthy ? f.health >= half : f.health < half;
    if (!fits) f.health = report.healthy ? 0.75f * f.max_health : 0.25f * f.max_health;
    return f;
}
}  // namespace

void remember_teammates() {
    const auto round = bc::round_number();
    if (round % 10 != 0) return;
    const auto& allies = roster_allies();
    auto& me = bc::self();
    for (int i = 0; i < static_cast<int>(allies.size()) && i < 8; ++i) {
        auto& mem = ally_memory[i];
        if (mem.seen && round - mem.round < 50) continue;
        if (allies[i] == me || in_view(allies[i])) mem = AllyMemory{fighter_state(allies[i]), round, true};
    }
}

TeamFightSetup team_fight_setup_here(const bc::champion** enemies_out, std::vector<SharedAlly>* shared_out, int* visible_count) {
    TeamFightSetup setup;
    if (shared_out) shared_out->clear();
    if (visible_count) *visible_count = 0;
    auto& me = bc::self();
    const auto here = me.position();
    const auto round = bc::round_number();
    // Each position is read once (every SDK read costs fuel).
    Seen ours[kTeamFightMaxSide], theirs[kTeamFightMaxSide];
    int ours_index[kTeamFightMaxSide];
    int n_ours = 0, n_theirs = 0;
    for (const auto& e : roster_enemies()) {
        if (n_theirs >= kTeamFightMaxSide || !in_view(e)) continue;
        const auto p = e.position();
        if (p.chebyshev_distance(here) <= constants::kTeamFightRadius) theirs[n_theirs++] = Seen{&e, p};
    }
    if (n_theirs == 0) return setup;
    const auto& allies = roster_allies();
    bool counted[8] = {};
    for (int i = 0; i < static_cast<int>(allies.size()) && n_ours < kTeamFightMaxSide; ++i) {
        const auto& a = allies[i];
        if (a == me) { ours_index[n_ours] = i; ours[n_ours++] = Seen{&a, here}; counted[i] = true; continue; }
        if (!in_view(a)) continue;
        const auto p = a.position();
        if (p.chebyshev_distance(here) <= constants::kTeamFightRadius) { ours_index[n_ours] = i; ours[n_ours++] = Seen{&a, p}; counted[i] = true; }
    }
    for (int i = 0; i < n_ours; ++i) {
        const Fighter f = fighter_state(*ours[i].c);
        if (ours_index[i] < 8) ally_memory[ours_index[i]] = AllyMemory{f, round, true};
        setup.us[setup.us_count++] = TeamMember{f};
        for (int j = 0; j < n_theirs; ++j) setup.distance[i][j] = static_cast<int>(ours[i].pos.chebyshev_distance(theirs[j].pos));
    }
    if (visible_count) *visible_count = setup.us_count;
    // Teammates we cannot see, from their position reports.
    if (constants::kTeamFightSharedAllies || shared_out) {
        for (int i = 0; i < static_cast<int>(allies.size()) && i < 8; ++i) {
            if (counted[i] || !ally_memory[i].seen) continue;
            const auto& report = ally_position(i);
            if (!report.known) continue;
            const int age = static_cast<int>(round - report.heard_round);
            const int to_me = static_cast<int>(report.pos.chebyshev_distance(here));
            if (age > 10 || to_me > 16) continue;
            bool alive = true;
            try { alive = allies[i].is_alive(); } catch (const std::exception&) {}
            if (!alive) continue;
            SharedAlly s;
            s.f = shared_fighter(ally_memory[i], report, round);
            for (int j = 0; j < n_theirs; ++j) s.distance[j] = static_cast<int>(report.pos.chebyshev_distance(theirs[j].pos));
            s.to_me = to_me;
            s.age = age;
            s.included = constants::kTeamFightSharedAllies && age <= constants::kAllyPositionMaxAge
                && to_me <= constants::kTeamFightSharedRadius && setup.us_count < kTeamFightMaxSide;
            if (s.included) {
                const int k = setup.us_count++;
                setup.us[k] = TeamMember{s.f};
                for (int j = 0; j < n_theirs; ++j) setup.distance[k][j] = s.distance[j];
            }
            if (shared_out) shared_out->push_back(s);
        }
    }
    for (int i = 0; i < n_theirs; ++i) {
        setup.enemy[setup.enemy_count++] = TeamMember{fighter_state(*theirs[i].c)};
        if (enemies_out) enemies_out[i] = theirs[i].c;
    }
    return setup;
}

TeamFightCall estimate_team_fight_here() {
    TeamFightCall call;
    if (!bc::self().is_alive()) return call;
    const bc::champion* enemies[kTeamFightMaxSide] = {};
    const auto setup = team_fight_setup_here(enemies);
    call.allies = setup.us_count;
    call.enemies = setup.enemy_count;
    if (setup.enemy_count == 0 || setup.us_count == 0) return call;
    call.estimate = constants::kTeamFightQuick
        ? estimate_team_fight_quick(setup, constants::kTeamFightMargin, constants::kTeamFightRoundCap, quick_options())
        : estimate_team_fight(setup, constants::kTeamFightMargin, constants::kTeamFightRoundCap);
    if (call.estimate.focus_target >= 0) call.target = enemies[call.estimate.focus_target];
    return call;
}

void call_team_fight(const bc::champion& target) {
    const int idx = roster_index(roster_enemies(), target);
    if (idx < 0 || !in_view(target)) return;
    const auto p = target.position();
    auto byte = [](int v) { return std::byte(static_cast<unsigned char>(std::clamp(v, 0, 255))); };
    const std::array<std::byte, 4> bytes{byte(idx + 1), byte(static_cast<int>(bc::round_number() & 0xff)), byte(p.x), byte(p.y)};
    bc::shared_memory.write(constants::kTeamFightCallByte, std::span{bytes});
}

void clear_team_fight_call() {
    const std::array<std::byte, 4> bytes{};
    bc::shared_memory.write(constants::kTeamFightCallByte, std::span{bytes});
}

std::optional<TeamFightOrder> team_fight_called() {
    std::array<std::byte, 4> bytes{};
    bc::shared_memory.read(constants::kTeamFightCallByte, std::span{bytes});
    const int idx = std::to_integer<int>(bytes[0]) - 1;
    const int age = (static_cast<int>(bc::round_number()) - std::to_integer<int>(bytes[1])) & 0xff;
    const auto& enemies = roster_enemies();
    if (idx < 0 || idx >= static_cast<int>(enemies.size()) || age > constants::kTeamFightCallRounds) return std::nullopt;
    const auto& target = enemies[idx];
    try {
        if (!target.is_alive()) return std::nullopt;
    } catch (const std::exception&) {}
    return TeamFightOrder{&target, bc::vec2{std::to_integer<int>(bytes[2]), std::to_integer<int>(bytes[3])}};
}

// ---- Test-build trace -------------------------------------------------------------------
namespace {
constexpr int kTraceContact = 4;          // a fight starts when an enemy is this close
constexpr std::uint32_t kTraceRounds = 20; // deaths counted for this many rounds after the call

struct Tracked {
    std::uint32_t start = 0;
    std::vector<int> allies, enemies;              // roster indices in the estimate
    std::vector<bc::vec2> ally_pos;                // where our champions in the estimate stood
    std::vector<bool> ally_died, enemy_died;
    std::vector<int> ally_death_round, enemy_death_round;   // rounds after the estimate; -1 alive
    std::vector<bool> ally_joined, enemy_joined;   // champions not in the estimate seen nearby later
};

// Every enemy this champion has seen: when, where and how strong (vision is not shared, so this
// is our own memory only). Test builds only.
struct Sighting {
    std::uint32_t round = 0;
    bc::vec2 pos{};
    Fighter f;
    bool seen = false;
};
Sighting sightings[16];
constexpr int kTraceMemoryRounds = 10;   // sightings this recent count as "nearby, unseen"
constexpr int kTraceMemoryRadius = 12;
std::optional<Tracked> tracked;
std::uint32_t next_allowed = 0;

const char* call_name(TeamCall c) { return c == TeamCall::Fight ? "fight" : c == TeamCall::Avoid ? "avoid" : "unsure"; }

bool alive(const bc::champion& c) {
    try { return c.is_alive(); } catch (const std::exception&) { return true; }   // unknown: not counted as a death
}

void update_deaths(std::uint32_t round) {
    const auto& A = roster_allies();
    const auto& E = roster_enemies();
    const int age = static_cast<int>(round - tracked->start);
    for (std::size_t i = 0; i < tracked->allies.size(); ++i)
        if (!tracked->ally_died[i] && !alive(A[tracked->allies[i]])) { tracked->ally_died[i] = true; tracked->ally_death_round[i] = age; }
    for (std::size_t i = 0; i < tracked->enemies.size(); ++i)
        if (!tracked->enemy_died[i] && !alive(E[tracked->enemies[i]])) { tracked->enemy_died[i] = true; tracked->enemy_death_round[i] = age; }
    // Reinforcements: champions outside the estimate that come within the fight radius.
    auto& me = bc::self();
    if (!me.is_alive()) return;
    const auto here = me.position();
    auto in_estimate = [](const std::vector<int>& v, int i) { return std::find(v.begin(), v.end(), i) != v.end(); };
    for (std::size_t i = 0; i < A.size() && i < tracked->ally_joined.size(); ++i)
        if (!in_estimate(tracked->allies, static_cast<int>(i)) && A[i] != me && in_view(A[i])
            && A[i].position().chebyshev_distance(here) <= constants::kTeamFightRadius) tracked->ally_joined[i] = true;
    for (std::size_t i = 0; i < E.size() && i < tracked->enemy_joined.size(); ++i)
        if (!in_estimate(tracked->enemies, static_cast<int>(i)) && in_view(E[i])
            && E[i].position().chebyshev_distance(here) <= constants::kTeamFightRadius) tracked->enemy_joined[i] = true;
}

void remember_enemies(std::uint32_t round) {
    const auto& E = roster_enemies();
    for (std::size_t i = 0; i < E.size() && i < 16; ++i)
        if (in_view(E[i])) sightings[i] = Sighting{round, E[i].position(), fighter_state(E[i]), true};
}
}  // namespace

void team_fight_trace_turn() {
    if constexpr (!constants::kTeamFightTrace) return;
    else {
        try {
            const auto round = bc::round_number();
            auto& me = bc::self();
            if (me.is_alive() && bc::fuel_remaining() > 30000) remember_enemies(round);
            if (tracked) {
                update_deaths(round);
                if (round - tracked->start < kTraceRounds) return;
                int ad = 0, ed = 0, aj = 0, ej = 0;
                for (bool d : tracked->ally_died) ad += d;
                for (bool d : tracked->enemy_died) ed += d;
                for (bool j : tracked->ally_joined) aj += j;
                for (bool j : tracked->enemy_joined) ej += j;
                std::cout << "TEAM_END r=" << round << " me=" << bc::to_string(me.champion_name()) << " start=" << tracked->start << " allies=" << tracked->allies.size()
                          << " enemies=" << tracked->enemies.size() << " ally_deaths=" << ad << " enemy_deaths=" << ed
                          << " joined_allies=" << aj << " joined_enemies=" << ej << " ally_death_rounds=";
                for (std::size_t i = 0; i < tracked->ally_death_round.size(); ++i) std::cout << (i ? "/" : "") << tracked->ally_death_round[i];
                std::cout << " enemy_death_rounds=";
                for (std::size_t i = 0; i < tracked->enemy_death_round.size(); ++i) std::cout << (i ? "/" : "") << tracked->enemy_death_round[i];
                std::cout << '\n';
                tracked.reset();
                next_allowed = round + 5;
                return;
            }
            if (!me.is_alive() || round < next_allowed || bc::fuel_remaining() < 40000) return;
            bool contact = false;
            for (const auto& e : roster_enemies())
                if (in_view(e) && e.position().chebyshev_distance(me.position()) <= kTraceContact) { contact = true; break; }
            if (!contact) return;
            const auto f0 = bc::fuel_remaining();
            const bc::champion* enemies[kTeamFightMaxSide] = {};
            static std::vector<SharedAlly> shared;
            int visible = 0;
            const auto setup = team_fight_setup_here(enemies, &shared, &visible);
            const auto f1 = bc::fuel_remaining();
            if (setup.us_count + setup.enemy_count < 3) return;   // 1v1s belong to the duel predictor
            // One logger per fight: the first ally in draft order among those in the estimate.
            Tracked t;
            t.start = round;
            const auto& A = roster_allies();
            const auto here = me.position();
            for (const auto& a : A) {
                if (a == me) { t.allies.push_back(roster_index(A, a)); t.ally_pos.push_back(here); continue; }
                if (!in_view(a)) continue;
                const auto p = a.position();
                if (p.chebyshev_distance(here) <= constants::kTeamFightRadius) { t.allies.push_back(roster_index(A, a)); t.ally_pos.push_back(p); }
            }
            if (t.allies.empty() || A[t.allies.front()] != me) return;
            for (int i = 0; i < setup.enemy_count; ++i) t.enemies.push_back(roster_index(roster_enemies(), *enemies[i]));
            t.ally_died.assign(t.allies.size(), false);
            t.enemy_died.assign(t.enemies.size(), false);
            t.ally_death_round.assign(t.allies.size(), -1);
            t.enemy_death_round.assign(t.enemies.size(), -1);
            t.ally_joined.assign(roster_allies().size(), false);
            t.enemy_joined.assign(roster_enemies().size(), false);
            // A turn that runs out of fuel pauses and resumes with a fresh allowance, which would
            // wrap these differences: report -1 then. The round version runs only with fuel to spare.
            auto spent = [](std::uint64_t a, std::uint64_t b) { return a >= b ? static_cast<long long>(a - b) : -1LL; };
            const auto f2 = bc::fuel_remaining();
            const auto quick = estimate_team_fight_quick(setup, constants::kTeamFightMargin, constants::kTeamFightRoundCap, quick_options());
            const auto f3 = bc::fuel_remaining();
            const bool full = f3 > 60000;
            TeamEstimate est;
            if (full) est = estimate_team_fight(setup, constants::kTeamFightMargin, constants::kTeamFightRoundCap);
            const auto f4 = bc::fuel_remaining();
            std::cout << "TEAM_PRED r=" << round << " me=" << bc::to_string(me.champion_name()) << " allies=" << setup.us_count
                      << " enemies=" << setup.enemy_count << " call=" << (full ? call_name(est.call) : "skip") << " runs=" << est.runs
                      << " quick_call=" << call_name(quick.call) << " quick_runs=" << quick.runs
                      << " setup_fuel=" << spent(f0, f1) << " est_fuel=" << (full ? spent(f3, f4) : -1LL)
                      << " quick_fuel=" << spent(f2, f3) << " support_w=" << constants::kTeamFightSupportWeight;
            // Full model input, so a call can be replayed offline: type,health,max,strength,focus,
            // shield,shield_turns,cd0,cd1,stunned,taken_mult; ours also list distances to each enemy.
            auto print = [](const Fighter& f) {
                std::cout << f.design->type << ',' << f.health << ',' << f.max_health << ',' << f.strength << ',' << f.focus << ','
                          << f.shield << ',' << f.shield_turns << ',' << f.cooldown[0] << ',' << f.cooldown[1] << ','
                          << f.stunned_rounds << ',' << f.damage_taken_mult;
            };
            for (int i = 0; i < setup.us_count; ++i) {
                std::cout << " U" << i << '=';
                print(setup.us[i].f);
                std::cout << ",d";
                for (int j = 0; j < setup.enemy_count; ++j) std::cout << (j ? "/" : "") << setup.distance[i][j];
            }
            // Teammates known only from position reports: S<k>=fighter,d<to each enemy>,m<to us>,a<age>,i<1 if in
            // the setup above (then also the U entries after the first vis= ones, in the same order)>.
            std::cout << " vis=" << visible;
            for (std::size_t k = 0; k < shared.size(); ++k) {
                std::cout << " S" << k << '=';
                print(shared[k].f);
                std::cout << ",d";
                for (int j = 0; j < setup.enemy_count; ++j) std::cout << (j ? "/" : "") << shared[k].distance[j];
                std::cout << ",m" << shared[k].to_me << ",a" << shared[k].age << ",i" << shared[k].included;
            }
            for (int i = 0; i < setup.enemy_count; ++i) {
                std::cout << " E" << i << '=';
                print(setup.enemy[i].f);
            }
            // Enemies we saw recently but cannot see in the fight now: last known stats, rounds since
            // seen, and the distance from each of ours to where they were (R<k>=fighter,a<age>,d<..>).
            {
                int k = 0;
                const auto& E = roster_enemies();
                for (std::size_t i = 0; i < E.size() && i < 16; ++i) {
                    if (std::find(t.enemies.begin(), t.enemies.end(), static_cast<int>(i)) != t.enemies.end()) continue;
                    const auto& sg = sightings[i];
                    if (!sg.seen || round - sg.round > static_cast<std::uint32_t>(kTraceMemoryRounds)) continue;
                    if (sg.pos.chebyshev_distance(me.position()) > kTraceMemoryRadius || !alive(E[i])) continue;
                    std::cout << " R" << k++ << '=';
                    print(sg.f);
                    std::cout << ",a" << (round - sg.round) << ",d";
                    for (std::size_t u = 0; u < t.ally_pos.size(); ++u)
                        std::cout << (u ? "/" : "") << t.ally_pos[u].chebyshev_distance(sg.pos);
                }
            }
            std::cout << '\n';
            tracked = std::move(t);
        } catch (const std::exception& e) {
            std::cout << "TEAM_TRACE_ERROR " << e.what() << '\n';
        }
    }
}
