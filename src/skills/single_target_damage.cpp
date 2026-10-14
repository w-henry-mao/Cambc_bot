#include "skill.h"
#include "skill_instances.h"
class SingleTargetDamage : public Skill {
public:
    bool can_use(const TurnContext& ctx) override { return false; }
    void use(TurnContext& ctx) override {}
};
SingleTargetDamage single_target_damage_instance;
