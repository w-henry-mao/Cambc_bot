#include <cambc.h>
#include "attack.h"
#include "../constants/brain_constants.h"
int AttackState::score(const TurnContext& ctx) {
    auto& me = cambc::self();
    if (!me.can_attack()) return 0;
    for (const auto& enemy : cambc::vision::champions(cambc::team::enemy)) {
        if (enemy.is_visible() && enemy.health() > 0 && me.attack_tiles().is_set(enemy.position())) {
            return constants::kAttackScore;
        }
    }
    return 0;
}
StateResult AttackState::execute(TurnContext& ctx, BotMemory& memory) {
    auto& me = cambc::self();
    for (const auto& enemy : cambc::vision::champions(cambc::team::enemy)) {
        if (enemy.is_visible() && enemy.health() > 0 && me.attack_tiles().is_set(enemy.position())) {
            me.attack(enemy);
            return StateResult::Continue;
        }
    }
    return StateResult::Done;
}
AttackState attack_state_instance;
