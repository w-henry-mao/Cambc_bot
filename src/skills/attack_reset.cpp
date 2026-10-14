#include "skill.h"
#include "skill_instances.h"
class AttackReset : public Skill {
public:
    bool can_use(const TurnContext& ctx) override { return false; }
    void use(TurnContext& ctx) override {}
};
AttackReset attack_reset_instance;
