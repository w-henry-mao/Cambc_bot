#include "context.h"
TurnContext::TurnContext(const ChampionProfile& profile_ref)
    : round_number(cambc::round_number()), profile(profile_ref), position(cambc::self().position()) {}
