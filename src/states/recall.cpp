#include <cambc.h>
#include "state_module.h"
#include "../constants/brain_constants.h"
#include "../constants/state_constants.h"
class RecallState : public StateModule {
public:
    int score(const TurnContext& ctx) override {
        auto& me = cambc::self();
        const bool at_spawn = cambc::map::spawn_region(cambc::team::ally).is_set(me.position());
        if (!at_spawn && me.health() < me.max_health() * constants::kRecallHpFraction) {
            return constants::kRecallScore;
        }
        return 0;
    }
    StateResult execute(TurnContext& ctx, BotMemory& memory) override {
        cambc::self().recall();
        return StateResult::Done;
    }
};
static RecallState recall_state_instance;
