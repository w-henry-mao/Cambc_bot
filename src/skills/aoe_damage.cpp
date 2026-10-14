#include "skill.h"
#include "skill_instances.h"
class AoeDamage : public Skill {
public:
    bool can_use(const TurnContext& ctx) override { return false; }
    void use(TurnContext& ctx) override {}
};
AoeDamage aoe_damage_instance;
