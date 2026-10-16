// Test-only rule probe. One champion (kFighterType) walks toward the enemy spawn until an
// enemy champion is within kStopDistance, then runs probe_act() every turn. Every other
// champion does nothing. Each turn logs state; each action logs its result.
#include <cambc.h>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
namespace bc = cambc;

struct Probe {
    int contact_turn = -1;              // turns since first contact (0 = first)
    std::optional<bc::champion> enemy;  // visible enemy champion this turn
    bc::champion& me = bc::self();
    void act(const char* what, bool ok, const std::string& err = "") {
        std::cout << "ACT r=" << bc::round_number() << " " << bc::to_string(me.champion_name()) << " " << what
                  << (ok ? " ok" : " no") << (err.empty() ? "" : " err=" + err) << "\n";
    }
    template <class F> void attempt(const char* what, F f) {
        try { act(what, f()); } catch (const bc::error& e) { act(what, false, std::to_string(int(e.code())) + ":" + e.message()); }
        catch (const std::exception& e) { act(what, false, e.what()); }
    }
    void attack() { attempt("attack", [&] { me.attack(*enemy); return true; }); }
    void cast_on_enemy(int s) { attempt(s ? "cast1_enemy" : "cast0_enemy", [&] { return me.abilities()[s].use(*enemy); }); }
    void cast_self(int s) { attempt(s ? "cast1_self" : "cast0_self", [&] { return me.abilities()[s].use(); }); }
    void cast_ground(int s, bc::vec2 p) { attempt(s ? "cast1_ground" : "cast0_ground", [&] { return me.abilities()[s].use(p); }); }
    // Step to the neighbouring free tile closest to the enemy (enemy tile itself is occupied).
    bool step_by(int sign) {
        const auto here = me.position();
        std::vector<bc::vec2> steps;
        for (int dx = -1; dx <= 1; ++dx) for (int dy = -1; dy <= 1; ++dy) if (dx || dy) steps.push_back({dx, dy});
        std::sort(steps.begin(), steps.end(), [&](bc::vec2 a, bc::vec2 b) {
            return sign * (here + a).chebyshev_distance(enemy->position()) < sign * (here + b).chebyshev_distance(enemy->position()); });
        const auto now = here.chebyshev_distance(enemy->position());
        for (const auto st : steps) {
            if (sign * (here + st).chebyshev_distance(enemy->position()) >= sign * now) break;
            try { if (me.move(st)) return true; } catch (const std::exception&) {}
        }
        return false;
    }
    void step_toward() { attempt("move_toward", [&] { return step_by(1); }); }
    void step_away() { attempt("move_away", [&] { return step_by(-1); }); }
    void dash_toward(int s) {
        // Two steps along the line to the enemy, stopping short of its tile.
        auto p = me.position();
        for (int i = 0; i < 2; ++i) {
            const auto d = enemy->position() - p;
            const bc::vec2 step{d.x > 0 ? 1 : d.x < 0 ? -1 : 0, d.y > 0 ? 1 : d.y < 0 ? -1 : 0};
            if ((p + step) == enemy->position()) break;
            p = p + step;
        }
        cast_ground(s, p);
    }
    int dist() const { return static_cast<int>(me.position().chebyshev_distance(enemy->position())); }
    void predict(int s) {
        try { std::cout << "PRED r=" << bc::round_number() << " slot" << s << " damage_amount=" << me.abilities()[s].damage_amount(*enemy) << "\n"; }
        catch (const std::exception& e) { std::cout << "PRED err " << e.what() << "\n"; }
    }
};

#include "probe_config.h"
#ifndef PROBE_HAS_READY
inline bool probe_ready(Probe&) { return true; }
#endif

static void log_state(Probe& p) {
    auto& me = p.me;
    const auto ab = me.abilities();
    const auto buffs = me.buffs();
    float shield = 0; int stun = 0;
    for (const auto& b : buffs) {
        if (const auto* s = std::get_if<bc::buff_shield>(&b)) shield = s->value;
        if (const auto* s = std::get_if<bc::buff_stun>(&b)) stun = s->duration;
    }
    std::cout << "P r=" << bc::round_number() << " " << bc::to_string(me.champion_name()) << " hp=" << me.health()
              << " sh=" << shield << " stun=" << stun << " cd=" << ab[0].cooldown() << "," << ab[1].cooldown()
              << " mv=" << me.can_move() << " at=" << me.can_attack() << " ca=" << me.can_cast()
              << " pos=" << bc::to_string(me.position()) << " ct=" << p.contact_turn << " gold=" << me.gold_count();
    if (p.enemy) std::cout << " | E " << bc::to_string(p.enemy->champion_name()) << " hp=" << p.enemy->health()
                           << " pos=" << bc::to_string(p.enemy->position()) << " dist=" << p.dist();
    std::cout << "\n";
}

static void play_draft() {
    const auto mine = bc::draft::current_draft(bc::team::ally);
    std::vector<bc::champion_draft> order{static_cast<bc::champion_draft>(kPick)};
    for (int i = 0; i < 11; ++i) order.push_back(static_cast<bc::champion_draft>(i));
    for (auto c : order)
        if (std::find(mine.begin(), mine.end(), c) == mine.end()) { bc::draft::pick(c); return; }
}

int main() {
    Probe* probe = nullptr;
    while (true) {
        try {
            if (bc::is_draft()) play_draft();
            else {
                auto& me = bc::self();
                if (static_cast<int>(me.champion_name()) == kFighterType && me.is_alive()) {
                    if (!probe) probe = new Probe();
                    auto& p = *probe;
                    p.enemy.reset();
                    for (const auto& e : bc::vision::champions(bc::team::enemy))
                        if (!p.enemy || e.position().chebyshev_distance(me.position()) < p.dist()) p.enemy = e;
                    if (p.enemy && (p.contact_turn >= 0 || p.dist() <= kStopDistance)) ++p.contact_turn;
                    log_state(p);
                    if (p.contact_turn >= 0) probe_act(p);
                    else if (probe_ready(p) && me.can_move()) {
                        static std::optional<bc::vec2> centre;
                        if (!centre) {
                            const auto open = ~(bc::map::walls() | bc::map::holes() | bc::map::altars()
                                | bc::map::spawn_region(bc::team::enemy) | bc::map::spawn_region(bc::team::ally));
                            const bc::vec2 mid{bc::map::width() / 2, bc::map::height() / 2};
                            for (const auto t : open.positions())
                                if (!centre || t.chebyshev_distance(mid) < centre->chebyshev_distance(mid)) centre = t;
                        }
                        me.move_towards(*centre);
                    }
                    if (p.contact_turn >= 0) log_state(p);
                }
            }
        } catch (const std::exception& e) { std::cout << "bot error: " << e.what() << std::endl; }
        std::cout.flush();
        bc::end_turn();
    }
}
