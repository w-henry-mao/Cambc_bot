#include <cambc.h>
#include "state_module.h"
#include "../nav/pathfinding.h"
#include "../constants/brain_constants.h"
class FarmState : public StateModule {
public:
    int score(const TurnContext& ctx) override { return constants::kFarmScore; }
    StateResult execute(TurnContext& ctx, BotMemory& memory) override {
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
            const auto state = group.state();
            if (group_visible && std::holds_alternative<cambc::map::camp_group_respawning>(state)) continue;
            goals |= group.spawn_tiles();
        }
        if (goals.is_set(me.position())) return StateResult::Continue;
        if (move_towards_closest(goals)) return StateResult::Continue;
        return StateResult::Failed;
    }
};
static FarmState farm_state_instance;
