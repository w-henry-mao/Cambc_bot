#include <cambc.h>
#include "state_module.h"
#include "../nav/pathfinding.h"
#include "../core/geometry.h"
#include "../constants/brain_constants.h"
class TributeState : public StateModule {
public:
    int score(const TurnContext& ctx) override {
        return cambc::self().gem_count() > 0 ? constants::kTributeScore : 0;
    }
    StateResult execute(TurnContext& ctx, BotMemory& memory) override {
        auto& me = cambc::self();
        for (auto altar : cambc::map::altars().positions()) {
            if (distance(me.position(), altar) <= cambc::constants::submit_gems_range && !cambc::vision::has_altar_lock(altar)) {
                me.submit_gems(altar);
                return StateResult::Done;
            }
        }
        cambc::bitboard goals;
        goals.clear();
        for (auto altar : cambc::map::altars().positions()) {
            if (!cambc::vision::has_altar_lock(altar)) {
                goals |= tiles_around(altar, cambc::constants::submit_gems_range);
            }
        }
        if (move_towards_closest(goals)) return StateResult::Continue;
        return StateResult::Failed;
    }
};
static TributeState tribute_state_instance;
