#include "team_fight.h"

TeamOutcome simulate_team_fight(const TeamFightSetup& setup, float our_damage_mult, float enemy_damage_mult, int round_cap) {
    return TeamOutcome{};
}

TeamEstimate estimate_team_fight(const TeamFightSetup& setup, float margin, int round_cap) {
    return TeamEstimate{};
}

TeamOutcome simulate_team_fight_quick(const TeamFightSetup& setup, float our_damage_mult, float enemy_damage_mult, int round_cap,
                                      const TeamFightOptions& options) {
    return TeamOutcome{};
}

TeamEstimate estimate_team_fight_quick(const TeamFightSetup& setup, float margin, int round_cap, const TeamFightOptions& options) {
    return TeamEstimate{};
}

int first_focus_target(const TeamFightSetup& setup) {
    return -1;
}
