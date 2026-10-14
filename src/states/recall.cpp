#include <cambc.h>
#include "recall.h"
#include "../constants/brain_constants.h"
#include "../constants/state_constants.h"
int RecallState::score(const TurnContext& ctx) {
    auto& me = cambc::self();
    const bool at_spawn = cambc::map::spawn_region(cambc::team::ally).is_set(me.position());
    if (!at_spawn && me.health() < me.max_health() * constants::kRecallHpFraction) {
        return constants::kRecallScore;
    }
    return 0;
}
StateResult RecallState::execute(TurnContext& ctx, BotMemory& memory) {
    cambc::self().recall();
    return StateResult::Done;
}
RecallState recall_state_instance;
