#include "champion_profile.h"
#include "../skills/attack_reset.h"
#include "../skills/dash.h"

ChampionProfile vesper_vale_profile = {
    .is_ranged = true,
    .skills = {&attack_reset_instance, &dash_instance},
    .linked_pair = false
};
