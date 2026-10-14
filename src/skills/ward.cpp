#include "ward.h"
bool Ward::can_use(const TurnContext& ctx) { return false; }
void Ward::use(TurnContext& ctx) {}
Ward ward_instance;
