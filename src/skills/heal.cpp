#include "heal.h"
bool Heal::can_use(const TurnContext& ctx) { return false; }
void Heal::use(TurnContext& ctx) {}
Heal heal_instance;
