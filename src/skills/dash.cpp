#include "skill.h"
#include "skill_instances.h"
class Dash : public Skill {
public:
    bool can_use(const TurnContext& ctx) override { return false; }
    void use(TurnContext& ctx) override {}
};
Dash dash_instance;
