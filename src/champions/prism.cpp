#include "champion_profile.h"
#include "../skills/damage_stun.h"
#include "../skills/aoe_damage.h"

ChampionProfile prism_profile = {
    .is_ranged = true,
    .skills = {&damage_stun_instance, &aoe_damage_instance},
    .linked_pair = false
};
