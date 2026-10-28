#include "champion_profile.h"
#include "../skills/enhanced_attack_reset.h"
#include "../skills/single_target_damage.h"

ChampionProfile marcella_profile = {
    .is_ranged = false,
    .skills = {&enhanced_attack_reset_instance, &single_target_damage_instance},
    .linked_pair = false
};
