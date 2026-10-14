#include "skill.h"
#include "skill_instances.h"
class Stun : public Skill {
public:
    bool can_use(const TurnContext& ctx) override { return false; }
    void use(TurnContext& ctx) override {}
};
Stun stun_instance;
