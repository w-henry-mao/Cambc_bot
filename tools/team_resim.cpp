// Offline check of team-fight estimate variants against real outcomes.
//
//   c++ -std=c++20 -O2 -Isrc tools/team_resim.cpp src/combat/team_fight.cpp \
//       src/combat/team_fight_quick.cpp src/combat/our_policy.cpp -o /tmp/team_resim
//   /tmp/team_resim data.tsv
//
// data.tsv comes from tools/team_dataset.py (one logged TEAM_PRED with its outcome per line).
// First it re-runs the shipped models on every logged input and reports how often they
// reproduce the call made in the match (anything well below 100% means the log and the model
// disagree, and nothing else here can be trusted). Then each variant is scored:
// - AUC: how well a continuous score from the even run (no margin) ranks fights we came out of
//   ahead (fewer of ours died) above fights we came out behind. 0.5 = no better than chance.
//   Independent of the margin, so variants compare fairly.
// - at margin 1.3: share of Fight calls where we lost fewer, of Avoid calls where we lost more,
//   and coverage (share of estimates that are not Unsure).
// Rows are split: tuning opponents, held-out opponents, and starter bots (reported apart).
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "combat/design_table.h"
#include "combat/team_fight.h"

namespace {
struct Row {
    std::string replay, ours, opp, map, line;
    int ally_deaths = 0, enemy_deaths = 0;
    int joined_allies = -1, joined_enemies = -1;   // -1: not logged
    TeamFightSetup setup;
    TeamFightSetup remembered;   // setup plus recently seen enemies (R fields), see below
    int remembered_count = 0;
    // Teammates known only from position reports (S fields): the setup above holds the visible
    // ones only (the first vis= U entries); these can be added back with any radius/age rule.
    struct Shared { Fighter f; int dist[kTeamFightMaxSide] = {}; int to_me = 0, age = 0; bool included = false; };
    std::vector<Shared> shared;
    bool has_shared_log = false;
    TeamFightSetup with_shared(int radius, int max_age, float weight = 1) const {
        TeamFightSetup s = setup;
        for (const auto& a : shared) {
            if (a.to_me > radius || a.age > max_age || s.us_count >= kTeamFightMaxSide) continue;
            const int k = s.us_count++;
            s.us[k].f = a.f;
            s.us[k].f.health *= weight; s.us[k].f.max_health *= weight; s.us[k].f.strength *= weight; s.us[k].f.focus *= weight;
            for (int j = 0; j < s.enemy_count; ++j) s.distance[k][j] = a.dist[j];
        }
        return s;
    }
    std::string call, quick_call;
    int label() const { return enemy_deaths > ally_deaths ? 1 : enemy_deaths < ally_deaths ? -1 : 0; }
};

std::string field(const std::string& line, const std::string& key) {
    const auto p = line.find(" " + key + "=");
    if (p == std::string::npos) return {};
    const auto b = p + key.size() + 2;
    return line.substr(b, line.find(' ', b) - b);
}

bool parse_fighter(const std::string& v, Fighter& f, std::vector<int>* dist) {
    std::vector<std::string> parts;
    std::stringstream ss(v);
    for (std::string x; std::getline(ss, x, ',');) parts.push_back(x);
    if (parts.size() < 11) return false;
    f.design = &design_for(std::atoi(parts[0].c_str()));
    f.health = std::strtof(parts[1].c_str(), nullptr);
    f.max_health = std::strtof(parts[2].c_str(), nullptr);
    f.strength = std::strtof(parts[3].c_str(), nullptr);
    f.focus = std::strtof(parts[4].c_str(), nullptr);
    f.shield = std::strtof(parts[5].c_str(), nullptr);
    f.shield_turns = std::atoi(parts[6].c_str());
    f.cooldown[0] = std::atoi(parts[7].c_str());
    f.cooldown[1] = std::atoi(parts[8].c_str());
    f.stunned_rounds = std::atoi(parts[9].c_str());
    f.damage_taken_mult = std::strtof(parts[10].c_str(), nullptr);
    if (dist && parts.size() >= 12 && parts[11].size() > 1 && parts[11][0] == 'd') {
        std::stringstream ds(parts[11].substr(1));
        for (std::string x; std::getline(ds, x, '/');) dist->push_back(std::atoi(x.c_str()));
    }
    return true;
}

std::vector<Row> load(const char* path) {
    std::vector<Row> rows;
    std::ifstream in(path);
    for (std::string l; std::getline(in, l);) {
        std::vector<std::string> cols;
        std::stringstream ss(l);
        for (std::string c; std::getline(ss, c, '\t');) cols.push_back(c);
        if (cols.size() < 9) continue;
        Row r;
        r.replay = cols[0]; r.ours = cols[1]; r.opp = cols[2]; r.map = cols[3];
        r.ally_deaths = std::atoi(cols[4].c_str()); r.enemy_deaths = std::atoi(cols[5].c_str());
        r.joined_allies = std::atoi(cols[6].c_str()); r.joined_enemies = std::atoi(cols[7].c_str());
        r.line = " " + cols[8] + " ";
        r.call = field(r.line, "call");
        r.quick_call = field(r.line, "quick_call");
        bool ok = true;
        for (int i = 0; i < kTeamFightMaxSide && ok; ++i) {
            const auto v = field(r.line, "U" + std::to_string(i));
            if (v.empty()) break;
            std::vector<int> d;
            ok = parse_fighter(v, r.setup.us[r.setup.us_count].f, &d);
            for (std::size_t j = 0; j < d.size() && j < kTeamFightMaxSide; ++j) r.setup.distance[r.setup.us_count][j] = d[j];
            ++r.setup.us_count;
        }
        // Logs with position sharing: keep only the visible U entries; reported ones come from S.
        const auto vis = field(r.line, "vis");
        if (!vis.empty()) {
            r.has_shared_log = true;
            r.setup.us_count = std::min(r.setup.us_count, std::atoi(vis.c_str()));
        }
        for (int i = 0; i < kTeamFightMaxSide && ok; ++i) {
            const auto v = field(r.line, "E" + std::to_string(i));
            if (v.empty()) break;
            ok = parse_fighter(v, r.setup.enemy[r.setup.enemy_count++].f, nullptr);
        }
        for (int k = 0; ok; ++k) {
            const auto v = field(r.line, "S" + std::to_string(k));
            if (v.empty()) break;
            Row::Shared a;
            const auto dpos = v.find(",d");
            if (dpos == std::string::npos || !parse_fighter(v.substr(0, dpos), a.f, nullptr)) break;
            std::stringstream ss(v.substr(dpos + 1));
            for (std::string x; std::getline(ss, x, ',');) {
                if (x.empty()) continue;
                if (x[0] == 'd') {
                    std::stringstream ds(x.substr(1));
                    int j = 0;
                    for (std::string y; std::getline(ds, y, '/') && j < kTeamFightMaxSide; ++j) a.dist[j] = std::atoi(y.c_str());
                } else if (x[0] == 'm') a.to_me = std::atoi(x.c_str() + 1);
                else if (x[0] == 'a') a.age = std::atoi(x.c_str() + 1);
                else if (x[0] == 'i') a.included = x[1] == '1';
            }
            r.shared.push_back(a);
        }
        // Recently seen enemies, as extra enemies: last known stats, cooldowns run down by the
        // rounds since, and distances from each of ours to where they were.
        r.remembered = r.setup;
        for (int i = 0; i < kTeamFightMaxSide && ok; ++i) {
            const auto v = field(r.line, "R" + std::to_string(i));
            if (v.empty() || r.remembered.enemy_count >= kTeamFightMaxSide) break;
            const auto a = v.find(",a");
            Fighter f;
            if (a == std::string::npos || !parse_fighter(v.substr(0, a), f, nullptr)) break;
            const int age = std::atoi(v.c_str() + a + 2);
            for (int& c : f.cooldown) c = std::max(0, c - age);
            const int e = r.remembered.enemy_count++;
            r.remembered.enemy[e].f = f;
            const auto dpos = v.find(",d", a);
            if (dpos != std::string::npos) {
                std::stringstream ds(v.substr(dpos + 2));
                int u = 0;
                for (std::string x; std::getline(ds, x, '/') && u < r.remembered.us_count; ++u) r.remembered.distance[u][e] = std::atoi(x.c_str());
            }
            ++r.remembered_count;
        }
        if (ok && r.setup.us_count > 0 && r.setup.enemy_count > 0) rows.push_back(r);
    }
    return rows;
}

// The live rule (constants/combat_constants.h): reported teammates within 12 tiles, reports at
// most 6 rounds old.
int constants_radius() { return 8; }
int constants_age() { return 3; }

const char* name(TeamCall c) { return c == TeamCall::Fight ? "fight" : c == TeamCall::Avoid ? "avoid" : "unsure"; }

bool is_starter(const Row& r) { return r.opp.rfind("starter", 0) == 0; }
bool held_out(const Row& r) { return r.opp.rfind("opp-rush", 0) == 0 || r.opp.rfind("opp-caller", 0) == 0; }

struct Prediction { TeamCall call; float score; int steps; };
using Model = std::function<Prediction(const Row&)>;

double auc(const std::vector<std::pair<float, int>>& xs) {
    // Mann-Whitney: P(score of a random "ahead" fight > score of a random "behind" fight).
    std::vector<std::pair<float, int>> v;
    for (const auto& x : xs) if (x.second != 0) v.push_back(x);
    std::sort(v.begin(), v.end());
    double rank_sum = 0, pos = 0, neg = 0;
    for (std::size_t i = 0; i < v.size();) {
        std::size_t j = i;
        while (j < v.size() && v[j].first == v[i].first) ++j;
        const double avg_rank = (static_cast<double>(i) + static_cast<double>(j) + 1) / 2;
        for (std::size_t k = i; k < j; ++k) if (v[k].second > 0) { rank_sum += avg_rank; ++pos; } else ++neg;
        i = j;
    }
    return pos > 0 && neg > 0 ? (rank_sum - pos * (pos + 1) / 2) / (pos * neg) : 0.5;
}

struct Score { double auc = 0; int fight = 0, fight_ok = 0, avoid = 0, avoid_ok = 0, n = 0; double steps = 0; };
Score evaluate(const std::vector<const Row*>& rows, const Model& m) {
    Score s;
    std::vector<std::pair<float, int>> xs;
    for (const auto* r : rows) {
        const auto p = m(*r);
        s.steps += p.steps;
        const int y = r->label();
        xs.push_back({p.score, y});
        if (r->ally_deaths == 0 && r->enemy_deaths == 0) continue;   // no deaths: no outcome
        ++s.n;
        if (p.call == TeamCall::Fight) { ++s.fight; s.fight_ok += y > 0; }
        if (p.call == TeamCall::Avoid) { ++s.avoid; s.avoid_ok += y < 0; }
    }
    s.auc = auc(xs);
    if (!rows.empty()) s.steps /= static_cast<double>(rows.size());
    return s;
}

void print(const char* label, const Score& s) {
    std::printf("    %-9s AUC %.3f  calls ok %3.0f%%  fight ok %3.0f%% (%4d)  avoid ok %3.0f%% (%4d)  coverage %3.0f%%  steps %.1f\n", label,
                s.auc, s.fight + s.avoid ? 100.0 * (s.fight_ok + s.avoid_ok) / (s.fight + s.avoid) : 0.0,
                s.fight ? 100.0 * s.fight_ok / s.fight : 0.0, s.fight, s.avoid ? 100.0 * s.avoid_ok / s.avoid : 0.0, s.avoid,
                s.n ? 100.0 * (s.fight + s.avoid) / s.n : 0.0, s.steps);
}

// Continuous score from one run: deaths difference first, health lost difference to break ties.
float outcome_score(const TeamOutcome& o) {
    return static_cast<float>(o.enemy_dead - o.our_dead) + 0.5f * (o.our_health_left - o.enemy_health_left);
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: team_resim data.tsv\n"); return 1; }
    const auto rows = load(argv[1]);
    std::printf("%zu estimates loaded\n", rows.size());

    // 1. Reproduction of the shipped models.
    int q_same = 0, q_n = 0, r_same = 0, r_n = 0;
    std::map<std::string, int> q_diff;
    for (const auto& r : rows) {
        if (!r.quick_call.empty()) {
            ++q_n;
            TeamFightOptions logged;   // the options the match ran with (older logs: defaults)
            const auto w = field(r.line, "support_w");
            if (!w.empty()) logged.support_weight = std::strtof(w.c_str(), nullptr);
            // The live setup: visible teammates plus the reported ones it counted (in log order).
            TeamFightSetup live = r.setup;
            for (const auto& a : r.shared) {
                if (!a.included || live.us_count >= kTeamFightMaxSide) continue;
                const int k = live.us_count++;
                live.us[k].f = a.f;
                for (int j = 0; j < live.enemy_count; ++j) live.distance[k][j] = a.dist[j];
            }
            const auto c = name(estimate_team_fight_quick(live, 1.3f, 40, logged).call);
            if (!r.call.empty() && r.call != "skip") { ++r_n; r_same += r.call == name(estimate_team_fight(live, 1.3f, 40).call); }
            if (r.quick_call == c) ++q_same; else ++q_diff[r.quick_call + "->" + c];
        }
    }
    std::printf("reproduced: quick %d/%d (%.2f%%), round %d/%d (%.2f%%)\n", q_same, q_n, 100.0 * q_same / std::max(1, q_n), r_same, r_n,
                100.0 * r_same / std::max(1, r_n));
    for (const auto& [k, v] : q_diff) std::printf("  quick mismatch %s: %d\n", k.c_str(), v);

    std::vector<const Row*> tune, held, starters;
    for (const auto& r : rows) (is_starter(r) ? starters : held_out(r) ? held : tune).push_back(&r);
    std::printf("tuning opponents %zu, held-out opponents (opp-rush, opp-caller) %zu, starters %zu\n\n", tune.size(), held.size(),
                starters.size());

    // 2. Variants.
    struct Variant { std::string name; Model m; };
    std::vector<Variant> variants;
    auto quick = [](TeamFightOptions o, int cap) {
        return [o, cap](const Row& r) {
            const auto& s = r.setup;
            const auto e = estimate_team_fight_quick(s, 1.3f, cap, o);
            const auto even = simulate_team_fight_quick(s, 1, 1, cap, o);
            return Prediction{e.call, outcome_score(even), even.steps + e.outcome.steps};
        };
    };
    // Baselines (score only): head count, total health, and health x damage (Lanchester-style).
    auto side_stats = [](const TeamMember* m, int n, float& hp, float& dps) {
        hp = dps = 0;
        for (int i = 0; i < n; ++i) {
            const auto& f = m[i].f;
            hp += (f.health + f.shield) / std::max(0.01f, f.damage_taken_mult);
            float d = f.strength;
            for (const auto& ab : f.design->slots) {
                const float cd = static_cast<float>(std::max(1, ab.cooldown));
                if ((ab.kind == AbilityKind::Damage || ab.kind == AbilityKind::AoeDamage || ab.kind == AbilityKind::Cc) && ab.targets_enemies)
                    d += ab.modifier * f.focus / cd;
                if (ab.kind == AbilityKind::AttackReset) d += f.strength / cd;
            }
            dps += d;
        }
    };
    variants.push_back({"baseline: head count", [](const Row& r) {
        const auto& s = r.setup;
        return Prediction{TeamCall::Unsure, static_cast<float>(s.us_count - s.enemy_count), 0};
    }});
    variants.push_back({"baseline: total health ratio", [side_stats](const Row& r) {
        const auto& s = r.setup;
        float uh, ud, eh, ed;
        side_stats(s.us, s.us_count, uh, ud);
        side_stats(s.enemy, s.enemy_count, eh, ed);
        return Prediction{TeamCall::Unsure, uh / std::max(1.0f, eh), 0};
    }});
    variants.push_back({"baseline: health x damage ratio", [side_stats](const Row& r) {
        const auto& s = r.setup;
        float uh, ud, eh, ed;
        side_stats(s.us, s.us_count, uh, ud);
        side_stats(s.enemy, s.enemy_count, eh, ed);
        return Prediction{TeamCall::Unsure, (uh * ud) / std::max(1.0f, eh * ed), 0};
    }});
    variants.push_back({"round version (shipped alt)", [](const Row& r) {
        const auto& s = r.setup;
        const auto e = estimate_team_fight(s, 1.3f, 40);
        const auto even = simulate_team_fight(s, 1, 1, 40);
        return Prediction{e.call, outcome_score(even), 0};
    }});
    variants.push_back({"quick (shipped)", quick({}, 40)});
    variants.push_back({"quick, cap 20", quick({}, 20)});
    for (float d : {0.0f, 1.0f, 2.0f, 4.0f}) {
        TeamFightOptions o;
        o.our_leave_below = 0.35f;
        o.leave_delay = d;
        variants.push_back({"quick, our recall 35%, delay " + std::to_string(static_cast<int>(d)), quick(o, 40)});
    }
    for (float d : {1.0f, 2.0f}) {
        TeamFightOptions o;
        o.our_leave_below = o.enemy_leave_below = 0.35f;
        o.leave_delay = d;
        variants.push_back({"quick, both recall 35%, delay " + std::to_string(static_cast<int>(d)), quick(o, 40)});
    }
    for (int j : {3, 4, 5, 6, 8}) {
        TeamFightOptions o;
        o.join_distance = j;
        variants.push_back({"quick, join within " + std::to_string(j), quick(o, 40)});
        o.our_leave_below = o.enemy_leave_below = 0.35f;
        o.leave_delay = 2;
        variants.push_back({"quick, join within " + std::to_string(j) + " + both recall delay 2", quick(o, 40)});
    }
    for (float w : {0.5f, 0.75f, 1.25f, 1.5f}) {
        TeamFightOptions o;
        o.spell_weight = w;
        variants.push_back({"quick, spell weight " + std::to_string(w).substr(0, 4), quick(o, 40)});
    }
    for (float w : {0.0f, 0.5f, 1.5f, 2.0f}) {
        TeamFightOptions o;
        o.stun_weight = w;
        variants.push_back({"quick, stun weight " + std::to_string(w).substr(0, 4), quick(o, 40)});
    }
    for (float w : {0.0f, 0.5f, 1.5f, 2.0f}) {
        TeamFightOptions o;
        o.support_weight = w;
        variants.push_back({"quick, support weight " + std::to_string(w).substr(0, 4), quick(o, 40)});
    }
    {
        TeamFightOptions o;
        o.support_weight = 0.5f;
        variants.push_back({"quick, support 0.5", quick(o, 40)});
        o.our_leave_below = o.enemy_leave_below = 0.35f;
        o.leave_delay = 2;
        variants.push_back({"quick, support 0.5 + both recall delay 2", quick(o, 40)});
    }
    // Recently seen enemies added to the fight (logs with R fields; others are unchanged).
    auto quick_mem = [](TeamFightOptions o, int max_age) {
        return [o, max_age](const Row& r) {
            const auto& s = max_age > 0 && r.remembered_count > 0 ? r.remembered : r.setup;
            const auto e = estimate_team_fight_quick(s, 1.3f, 40, o);
            const auto even = simulate_team_fight_quick(s, 1, 1, 40, o);
            return Prediction{e.call, outcome_score(even), even.steps + e.outcome.steps};
        };
    };
    variants.push_back({"quick + remembered enemies", quick_mem({}, 10)});
    {
        TeamFightOptions o;
        o.support_weight = 0.5f;
        variants.push_back({"quick + remembered enemies + support 0.5", quick_mem(o, 10)});
        o.our_leave_below = o.enemy_leave_below = 0.35f;
        o.leave_delay = 2;
        variants.push_back({"quick + remembered enemies + support 0.5 + both recall delay 2", quick_mem(o, 10)});
    }
    // Teammates from position reports (logs with S fields): radius and report age.
    auto quick_shared = [](TeamFightOptions o, int radius, int max_age, float weight = 1) {
        return [o, radius, max_age, weight](const Row& r) {
            const auto s = r.with_shared(radius, max_age, weight);
            const auto e = estimate_team_fight_quick(s, 1.3f, 40, o);
            const auto even = simulate_team_fight_quick(s, 1, 1, 40, o);
            return Prediction{e.call, outcome_score(even), even.steps + e.outcome.steps};
        };
    };
    {
        TeamFightOptions o;
        o.support_weight = 0.5f;
        variants.push_back({"shared: none (visible only)", quick_shared(o, -1, 0)});
        for (int radius : {8, 12, 16})
            for (int age : {3, 6})
                for (float w : {0.25f, 0.5f, 0.75f, 1.0f})
                    variants.push_back({"shared: within " + std::to_string(radius) + ", age <= " + std::to_string(age) + ", weight " +
                                            std::to_string(w).substr(0, 4), quick_shared(o, radius, age, w)});
    }
    if (argc > 2) {   // optional filter: only variants whose name contains argv[2]
        std::vector<Variant> keep;
        for (auto& v : variants) if (v.name.find(argv[2]) != std::string::npos) keep.push_back(v);
        variants.swap(keep);
    }
    if (argc > 3 && std::string(argv[3]) == "builds") {
        // Each of our builds scored on the calls it logged (the live model), outcomes over the next
        // 10 and 20 rounds. Non-starter opponents.
        auto deaths = [](const std::string& v, int w) {
            int n = 0;
            std::stringstream ss(v);
            for (std::string x; std::getline(ss, x, '/');) { const int d = std::atoi(x.c_str()); n += d >= 0 && d <= w; }
            return n;
        };
        std::map<std::string, std::vector<Row>> by;
        for (int w : {10, 20})
            for (const auto& r : rows) {
                if (is_starter(r)) continue;
                const auto a = field(r.line, "ally_death_rounds"), e = field(r.line, "enemy_death_rounds");
                if (a.empty() || e.empty()) continue;
                Row c = r;
                c.ally_deaths = deaths(a, w);
                c.enemy_deaths = deaths(e, w);
                by[r.ours + (w == 10 ? "  @10" : "  @20")].push_back(c);
            }
        const Model logged = [](const Row& r) {
            const auto c = r.quick_call;
            return Prediction{c == "fight" ? TeamCall::Fight : c == "avoid" ? TeamCall::Avoid : TeamCall::Unsure, 0, 0};
        };
        for (const auto& [k, v] : by) {
            std::vector<const Row*> p;
            for (const auto& x : v) p.push_back(&x);
            std::printf("%-22s", k.c_str());
            print("", evaluate(p, logged));
        }
        return 0;
    }
    if (argc > 3 && std::string(argv[3]) == "changed") {
        // Calls that change when reported teammates are added: which version was right.
        TeamFightOptions o;
        o.support_weight = 0.5f;
        for (auto [radius, age] : {std::pair{8, 3}, std::pair{12, 6}}) {
            std::map<std::string, std::array<int, 3>> moved;   // "before->after": we lost fewer / same / more
            int with_reports = 0, joined_when_reported = 0, reported = 0;
            for (const auto& r : rows) {
                if (is_starter(r) || (r.ally_deaths == 0 && r.enemy_deaths == 0)) continue;
                const auto before = estimate_team_fight_quick(r.setup, 1.3f, 40, o).call;
                const auto ws = r.with_shared(radius, age);
                if (ws.us_count > r.setup.us_count) {
                    ++reported;
                    joined_when_reported += r.joined_allies > 0;
                }
                const auto after = estimate_team_fight_quick(ws, 1.3f, 40, o).call;
                if (before == after) continue;
                ++with_reports;
                moved[std::string(name(before)) + "->" + name(after)][r.label() + 1]++;
            }
            std::printf("reported teammates within %d tiles, reports <= %d rounds old: %d calls changed\n", radius, age, with_reports);
            std::printf("  estimates with a reported teammate added: %d, of which a teammate actually joined: %d%%\n", reported,
                        reported ? 100 * joined_when_reported / reported : 0);
            for (const auto& [k, c] : moved) {
                const int n = c[0] + c[1] + c[2];
                std::printf("  %-16s %4d   we lost fewer %3d%%  same %3d%%  more %3d%%\n", k.c_str(), n, 100 * c[2] / n, 100 * c[1] / n, 100 * c[0] / n);
            }
        }
        return 0;
    }
    if (argc > 3 && std::string(argv[3]) == "champions") {
        // Shipped model (live setup) per champion type on each side: decisive calls right, over the
        // next 10 and 20 rounds. Non-starter opponents.
        static const char* names[] = {"ryth", "vesper", "vendetta", "cassi", "issac", "stellar", "prism", "marcella", "willow", "ren", "varg", "tavi"};
        auto deaths = [](const std::string& v, int w) {
            int n = 0;
            std::stringstream ss(v);
            for (std::string x; std::getline(ss, x, '/');) { const int d = std::atoi(x.c_str()); n += d >= 0 && d <= w; }
            return n;
        };
        TeamFightOptions o;
        o.support_weight = 0.5f;
        int ok[2][12][2] = {}, n[2][12][2] = {};
        for (const auto& r : rows) {
            if (is_starter(r)) continue;
            const auto a = field(r.line, "ally_death_rounds"), e = field(r.line, "enemy_death_rounds");
            if (a.empty() || e.empty()) continue;
            TeamFightSetup live = r.with_shared(constants_radius(), constants_age());
            const auto call = estimate_team_fight_quick(live, 1.3f, 40, o).call;
            if (call == TeamCall::Unsure) continue;
            for (int w = 0; w < 2; ++w) {
                const int win = w == 0 ? 10 : 20;
                const int ad = deaths(a, win), ed = deaths(e, win);
                if (ad == ed) continue;
                const bool right = (call == TeamCall::Fight) == (ed > ad);
                bool seen[2][12] = {};
                for (int i = 0; i < r.setup.us_count; ++i) seen[0][r.setup.us[i].f.design->type] = true;
                for (int i = 0; i < r.setup.enemy_count; ++i) seen[1][r.setup.enemy[i].f.design->type] = true;
                for (int side = 0; side < 2; ++side)
                    for (int t = 0; t < 12; ++t)
                        if (seen[side][t]) { ++n[side][t][w]; ok[side][t][w] += right; }
            }
        }
        std::printf("decisive calls right, by champion in the fight (10 rounds / 20 rounds, calls)\n");
        for (int side = 0; side < 2; ++side) {
            std::printf("  %s side\n", side == 0 ? "our" : "enemy");
            for (int t = 0; t < 12; ++t)
                if (n[side][t][1])
                    std::printf("    %-9s %3d%% / %3d%%   (%d / %d)\n", names[t], n[side][t][0] ? 100 * ok[side][t][0] / n[side][t][0] : 0,
                                100 * ok[side][t][1] / n[side][t][1], n[side][t][0], n[side][t][1]);
        }
        return 0;
    }
    if (argc > 3 && std::string(argv[3]) == "windows") {
        // Outcome counted over the first W rounds after the estimate (death rounds logged).
        auto deaths = [](const std::string& v, int w) {
            int n = 0;
            std::stringstream ss(v);
            for (std::string x; std::getline(ss, x, '/');) { const int d = std::atoi(x.c_str()); n += d >= 0 && d <= w; }
            return n;
        };
        TeamFightOptions o;
        o.support_weight = 0.5f;
        const Model m = quick(o, 40);
        for (int w : {5, 10, 15, 20}) {
            std::vector<Row> copy;
            for (const auto& r : rows) {
                const auto a = field(r.line, "ally_death_rounds"), e = field(r.line, "enemy_death_rounds");
                if (is_starter(r) || a.empty() || e.empty()) continue;
                Row c = r;
                c.ally_deaths = deaths(a, w);
                c.enemy_deaths = deaths(e, w);
                copy.push_back(c);
            }
            std::vector<const Row*> ptrs;
            for (const auto& c : copy) ptrs.push_back(&c);
            char label[16];
            std::snprintf(label, sizeof label, "%d rounds", w);
            print(label, evaluate(ptrs, m));
        }
        return 0;
    }
    if (argc > 3 && std::string(argv[3]) == "margins") {
        // Accuracy against coverage for each margin (the team's choice), non-starter opponents.
        std::vector<const Row*> all;
        for (const auto& r : rows) if (!is_starter(r)) all.push_back(&r);
        for (float w : {1.0f, 0.5f}) {
            std::printf("support weight %.1f\n", w);
            for (float margin : {1.0f, 1.1f, 1.2f, 1.3f, 1.5f, 1.75f, 2.0f}) {
                TeamFightOptions o;
                o.support_weight = w;
                const Model m = [o, margin](const Row& r) {
                    const auto e = estimate_team_fight_quick(r.setup, margin, 40, o);
                    return Prediction{e.call, 0, e.outcome.steps};
                };
                char label[16];
                std::snprintf(label, sizeof label, "x%.2f", margin);
                print(label, evaluate(all, m));
            }
        }
        return 0;
    }
    if (argc > 3 && std::string(argv[3]) == "joins") {
        // Shipped quick model on fights where nobody joined later vs fights where enemies or allies did.
        std::vector<const Row*> none, enemies_joined, allies_joined, unseen_known;
        for (const auto& r : rows) {
            if (is_starter(r) || r.joined_enemies < 0) continue;
            if (r.joined_enemies == 0 && r.joined_allies == 0) none.push_back(&r);
            if (r.joined_enemies > 0) enemies_joined.push_back(&r);
            if (r.joined_allies > 0) allies_joined.push_back(&r);
            if (r.remembered_count > 0) unseen_known.push_back(&r);
        }
        const Model m = quick({}, 40);
        std::printf("shipped quick model, non-starter opponents\n");
        print("no joins", evaluate(none, m));
        print("enemy+", evaluate(enemies_joined, m));
        print("ally+", evaluate(allies_joined, m));
        print("R logged", evaluate(unseen_known, m));
        print("R used", evaluate(unseen_known, quick_mem({}, 10)));
        std::printf("  (%zu / %zu / %zu / %zu rows)\n", none.size(), enemies_joined.size(), allies_joined.size(), unseen_known.size());
        return 0;
    }
    if (argc > 3 && std::string(argv[3]) == "calibration") {
        // Outcome by the even run's predicted deaths difference (non-starter rows with deaths).
        std::map<int, std::array<int, 3>> bins;
        for (const auto& r : rows) {
            if (is_starter(r) || (r.ally_deaths == 0 && r.enemy_deaths == 0)) continue;
            const auto o = simulate_team_fight_quick(r.setup, 1, 1, 40);
            const int b = std::clamp(o.enemy_dead - o.our_dead, -3, 3);
            bins[b][r.label() + 1]++;
        }
        std::printf("predicted (their deaths - ours) -> share where we lost fewer / same / more\n");
        for (const auto& [b, c] : bins) {
            const int n = c[0] + c[1] + c[2];
            std::printf("  %+d: %5d   fewer %3d%%  same %3d%%  more %3d%%\n", b, n, 100 * c[2] / n, 100 * c[1] / n, 100 * c[0] / n);
        }
        return 0;
    }
    for (const auto& v : variants) {
        std::printf("%s\n", v.name.c_str());
        print("tuning", evaluate(tune, v.m));
        print("held-out", evaluate(held, v.m));
        print("starters", evaluate(starters, v.m));
    }
}
