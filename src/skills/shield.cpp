#include "skill.h"
#include "skill_instances.h"
class Shield : public Skill {
public:
    bool can_use(const TurnContext& ctx) override { return false; }
    void use(TurnContext& ctx) override {}
};
Shield shield_instance;
