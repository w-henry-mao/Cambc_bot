#include <cambc.h>
#include "state_module.h"
#include "../constants/brain_constants.h"
class AttackState : public StateModule {
public:
    int score(const TurnContext& ctx) override {
        auto& me = cambc::self();
        if (!me.can_attack()) return 0;
        for (const auto& enemy : cambc::vision::champions(cambc::team::enemy)) {
            if (enemy.is_visible() && enemy.health() > 0 && me.attack_tiles().is_set(enemy.position())) {
                return constants::kAttackScore;
            }
        }
        return 0;
    }
    StateResult execute(TurnContext& ctx, BotMemory& memory) override {
        auto& me = cambc::self();
        for (const auto& enemy : cambc::vision::champions(cambc::team::enemy)) {
            if (enemy.is_visible() && enemy.health() > 0 && me.attack_tiles().is_set(enemy.position())) {
                me.attack(enemy);
                return StateResult::Continue;
            }
        }
        return StateResult::Done;
    }
};
static AttackState attack_state_instance;
