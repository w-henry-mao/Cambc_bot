#include "context.h"

TurnContext::TurnContext(const ChampionProfile& profile_ref)
    : round_number(0), profile(profile_ref), position{0, 0} {
}
