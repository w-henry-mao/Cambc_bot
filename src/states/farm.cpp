#include <cambc.h>
#include "farm.h"
#include "../nav/pathfinding.h"
#include "../constants/brain_constants.h"
int FarmState::score(const TurnContext& ctx) { return constants::kFarmScore; }
StateResult FarmState::execute(TurnContext& ctx, BotMemory& memory) {
    auto& me = cambc::self();
    for (const auto& camp : cambc::vision::camps()) {
        if (me.can_attack() && camp.is_visible() && camp.health() > 0 && me.attack_tiles().is_set(camp.position())) {
            me.attack(camp);
            return StateResult::Continue;
        }
    }
    cambc::bitboard goals;
    goals.clear();
    const auto visible = cambc::vision::visible_tiles();
    for (const auto& group : cambc::map::camp_groups()) {
        const bool group_visible = !(group.spawn_tiles() & visible).is_empty();
        // state() throws for groups out of vision, so only ask about visible ones.
        if (group_visible && std::holds_alternative<cambc::map::camp_group_respawning>(group.state())) continue;
        goals |= group.spawn_tiles();
    }
    if (goals.is_set(me.position())) return StateResult::Continue;
    if (move_towards_closest(goals)) return StateResult::Continue;
    return StateResult::Failed;
}
FarmState farm_state_instance;
