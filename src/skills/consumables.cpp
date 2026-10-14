#include "consumables.h"
bool Consumables::can_use(const TurnContext& ctx) { return false; }
void Consumables::use(TurnContext& ctx) {}
Consumables consumables_instance;
