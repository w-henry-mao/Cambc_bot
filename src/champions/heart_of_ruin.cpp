#include "champion_profile.h"
#include "../skills/shield.h"
#include "../skills/damage_stun_dash.h"

ChampionProfile heart_of_ruin_profile = {
    .is_ranged = false,
    .skills = {&shield_instance, &damage_stun_dash_instance},
    .linked_pair = false
};
