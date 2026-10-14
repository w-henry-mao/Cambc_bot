#pragma once
#include <cambc.h>
struct TurnContext;
class Skill {
public:
    virtual ~Skill() = default;
    virtual bool can_use(const TurnContext& ctx) = 0;
    virtual void use(TurnContext& ctx) = 0;
};
