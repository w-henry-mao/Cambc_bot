#include "skill.h"
#include "skill_instances.h"
class RangedAttack : public Skill {
public:
    bool can_use(const TurnContext& ctx) override { return false; }
    void use(TurnContext& ctx) override {}
};
RangedAttack ranged_attack_instance;
