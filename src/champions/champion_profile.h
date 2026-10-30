#pragma once
#include <cambc.h>
struct ChampionProfile {
    bool is_ranged;
    // Focus multipliers are added to the farming damage estimate; use zero for non-damaging skills.
    float ability1_damage_multiplier;
    float ability2_damage_multiplier;
    // An attack reset contributes one extra basic attack to the damage estimate.
    bool ability1_is_attack_reset;
    bool ability2_is_attack_reset;
};
