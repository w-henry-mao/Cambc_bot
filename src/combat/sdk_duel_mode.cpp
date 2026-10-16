#include "sdk_duel_mode.h"
#include <cambc.h>
#include <optional>
#include "design_table.h"
#include "our_policy.h"
#include "sdk_trace.h"
#include "../constants/combat_constants.h"
#include "../core/roster.h"

namespace {
namespace bc = cambc;
constexpr int kContact = 4;

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
}  // namespace

bool duel_mode_turn() {
    if constexpr (!constants::kDuelMode) return false;
    else {
        auto& me = bc::self();
        if (static_cast<int>(me.champion_name()) != constants::kDuelFighterType) return true;   // others stay home
        std::optional<bc::champion> enemy;
        for (const auto& e : bc::vision::champions(bc::team::enemy))
            if (in_view(e) && (!enemy || e.position().chebyshev_distance(me.position()) < enemy->position().chebyshev_distance(me.position())))
                enemy = e;
        const int dist = enemy ? static_cast<int>(enemy->position().chebyshev_distance(me.position())) : 1 << 20;
        if (dist <= reach(design_for(static_cast<int>(me.champion_name())))) return false;   // normal attack code
        if (dist <= kContact) { combat_trace_turn_start(); return true; }                  // wait, like the real bot
        if (me.can_move()) me.move_towards(map_centre());
        return true;
    }
}

void duel_mode_end_of_turn() {
    if constexpr (!constants::kDuelMode) return;
    else {
        if (static_cast<int>(bc::self().champion_name()) != constants::kDuelFighterType) return;
        if (combat_trace_fight_finished() || bc::round_number() > 400) bc::resign();
    }
}
