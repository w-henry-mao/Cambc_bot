#include "stun.h"
bool Stun::can_use(const TurnContext& ctx) { return false; }
void Stun::use(TurnContext& ctx) {}
Stun stun_instance;
