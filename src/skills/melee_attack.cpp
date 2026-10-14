#include "skill.h"
#include "skill_instances.h"
class MeleeAttack : public Skill {
public:
    bool can_use(const TurnContext& ctx) override { return false; }
    void use(TurnContext& ctx) override {}
};
MeleeAttack melee_attack_instance;
