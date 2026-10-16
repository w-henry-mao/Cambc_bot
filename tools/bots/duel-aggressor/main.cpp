// Test-only opponent: one champion (kAggType) walks to the map centre and fights the nearest
// enemy champion by playing the fight model's max-aggression plan (choose_enemy_plan) with real
// SDK calls, in the same action order as the simulator. Other champions stay home.
#include <cambc.h>
#include <algorithm>
#include <iostream>
#include <optional>
#include <vector>
#include "agg_config.h"
#include "combat/sdk_adapter.h"
#include "combat/simulator.h"
#include "core/roster.h"
namespace bc = cambc;

namespace {
bc::vec2 map_centre() {
    static std::optional<bc::vec2> centre;
    if (!centre) {
        const auto open = ~(bc::map::walls() | bc::map::holes() | bc::map::altars()
            | bc::map::spawn_region(bc::team::enemy) | bc::map::spawn_region(bc::team::ally));
        const bc::vec2 mid{bc::map::width() / 2, bc::map::height() / 2};
        for (const auto t : open.positions())
            if (!centre || t.chebyshev_distance(mid) < centre->chebyshev_distance(mid)) centre = t;
    }
    return *centre;
}

struct Turn {
    bc::champion& me = bc::self();
    bc::champion target;
    int dist() const { return static_cast<int>(me.position().chebyshev_distance(target.position())); }
    template <class F> bool attempt(F f) {
        try { return f(); } catch (const std::exception& e) { std::cout << "AGG_ERR " << e.what() << '\n'; return false; }
    }
    bool walk() {   // one step that brings the Chebyshev distance down by one
        if (dist() <= 1 || !me.can_move()) return false;
        const auto here = me.position();
        const int now = dist();
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy) {
                if (!dx && !dy) continue;
                const bc::vec2 step{dx, dy};
                if (static_cast<int>((here + step).chebyshev_distance(target.position())) != now - 1) continue;
                try { if (me.move(step)) return true; } catch (const std::exception&) {}
            }
        return false;
    }
    void attack() {
        if (me.can_attack() && dist() <= me.attack_range()) attempt([&] { me.attack(target); return true; });
    }
    void close_and_attack() {
        if (dist() > me.attack_range()) walk();
        attack();
    }
    void play(const Plan& p) {
        const auto ab = me.abilities();
        switch (p.kind) {
            case PlanKind::Plain: close_and_attack(); break;
            case PlanKind::Offensive: {
                const auto& d = setup_design();
                const auto& a = d.slots[p.slot];
                if (dist() > a.range && !a.excludes_walking) walk();
                if (dist() <= a.range) attempt([&] { return ab[p.slot].use(target); });
                close_and_attack();
                break;
            }
            case PlanKind::Reset:
                close_and_attack();
                attempt([&] { return ab[p.slot].use(); });
                attack();
                break;
            case PlanKind::Dash: {
                auto pos = me.position();
                for (int i = 0; i < 2; ++i) {   // up to two steps toward the target, stopping short of it
                    const auto d = target.position() - pos;
                    const bc::vec2 step{d.x > 0 ? 1 : d.x < 0 ? -1 : 0, d.y > 0 ? 1 : d.y < 0 ? -1 : 0};
                    if ((pos + step).chebyshev_distance(target.position()) < 1) break;
                    if ((pos + step) == target.position()) break;
                    pos = pos + step;
                }
                attempt([&] { return ab[p.slot].use(pos); });
                close_and_attack();
                break;
            }
            case PlanKind::Shield:
                attempt([&] { return ab[p.slot].use(); });
                close_and_attack();
                break;
        }
    }
    const ChampionDesign& setup_design() const;
};
}  // namespace

#include "combat/design_table.h"
const ChampionDesign& Turn::setup_design() const { return design_for(static_cast<int>(me.champion_name())); }

static void play_draft() {
    const auto mine = bc::draft::current_draft(bc::team::ally);
    std::vector<bc::champion_draft> order{static_cast<bc::champion_draft>(kAggPick)};
    for (int i = 0; i < 11; ++i) order.push_back(static_cast<bc::champion_draft>(i));
    for (auto c : order)
        if (std::find(mine.begin(), mine.end(), c) == mine.end()) { bc::draft::pick(c); return; }
}

int main() {
    while (true) {
        try {
            if (bc::is_draft()) play_draft();
            else {
                auto& me = bc::self();
                if (me.is_alive() && static_cast<int>(me.champion_name()) == kAggType && !me.is_recalling()) {
                    std::optional<bc::champion> enemy;
                    for (const auto& e : bc::vision::champions(bc::team::enemy))
                        if (in_view(e) && (!enemy || e.position().chebyshev_distance(me.position()) < enemy->position().chebyshev_distance(me.position())))
                            enemy = e;
                    if (enemy && enemy->position().chebyshev_distance(me.position()) <= 8) {
                        Turn t{me, *enemy};
                        const auto setup = duel_setup(*enemy, me);   // opponent plays "our" side of the model
                        const auto plan = choose_enemy_plan(setup);
                        std::cout << "AGG r=" << bc::round_number() << " plan=" << static_cast<int>(plan.kind) << " slot=" << plan.slot
                                  << " dist=" << t.dist() << '\n';
                        t.play(plan);
                    } else if (me.can_move()) {
                        me.move_towards(map_centre());
                    }
                }
            }
        } catch (const std::exception& e) { std::cout << "bot error: " << e.what() << std::endl; }
        std::cout.flush();
        bc::end_turn();
    }
}
