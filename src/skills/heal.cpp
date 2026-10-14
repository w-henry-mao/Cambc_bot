#include "skill.h"
#include "skill_instances.h"
class Heal : public Skill {
public:
    bool can_use(const TurnContext& ctx) override { return false; }
    void use(TurnContext& ctx) override {}
};
Heal heal_instance;
