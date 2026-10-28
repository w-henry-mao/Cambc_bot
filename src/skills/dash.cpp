#include "dash.h"

bool Dash::can_use(const TurnContext& ctx) {
    return false;
}

void Dash::use(TurnContext& ctx) {
}

Dash dash_instance;
