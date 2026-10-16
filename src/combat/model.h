#pragma once
// Pure fight-model types. No cambc.h here: this compiles natively for the host tests.
// Kinds and passives mirror the SDK's ability_kind and passive_type (same order).
#include <cstdint>

enum class AbilityKind : std::uint8_t { None, AttackReset, Damage, AoeDamage, Cc, Heal, Dash, Shield };
enum class Passive : std::uint8_t { None, BonusGold, RemoteShop, GemFocus, SplashAttack, HealthStrength,
                                    ResetCooldownsOnKill, Retaliation, StunImmunity };

struct AbilityDesign {
    AbilityKind kind = AbilityKind::None;
    float range = 0, modifier = 0, radius = 0;   // range/radius in tiles; damage/heal/shield = modifier x Focus
    int cooldown = 0;                            // owner turn ends
    int duration = 0;                            // stun rounds (Cc, AoeDamage) or shield turns (Shield)
    bool excludes_walking = false;
    bool enhanced_attack = false;                // AttackReset whose next attack stuns
    bool targets_enemies = false, targets_self = false, targets_allies = false;
};

struct ChampionDesign {
    int type = -1;                               // cambc::champion_type value
    bool ranged = false;
    int attack_range = 1, move_speed = 1, vision_radius = 8;
    float base_health = 0, health_growth = 0, base_strength = 0, strength_growth = 0;
    float base_focus = 0, focus_growth = 0;
    AbilityDesign slots[2];
    Passive passive = Passive::None;
};

// One side of a duel at the moment of the call (live numbers, not design values).
struct Fighter {
    const ChampionDesign* design = nullptr;
    float health = 0, max_health = 0, strength = 0, focus = 0;
    float shield = 0;              // shield points currently active
    int shield_turns = 0;          // owner turn ends until the shield expires
    int cooldown[2] = {0, 0};      // remaining owner turn ends; 0 = ready
    int stunned_rounds = 0;        // already stunned at the start of the fight
    float damage_taken_mult = 1;   // e.g. 0.9 with a Durability emblem
};

struct DuelSetup {
    Fighter us, enemy;
    int walk_distance = 0;              // steps over terrain between the two champions
    bool enemy_acted_this_round = false; // the call happens on our turn; did the enemy go first this round?
    bool same_turn_position = true;      // same position in each team's turn order (see turn_order.h)
    bool our_turn_now = true;            // false when predicting for an ally that is not acting now
    float camp_damage_to_us_per_round = 0, camp_damage_to_enemy_per_round = 0;
};

struct FightOptions {
    bool include_camps = false;
    int round_cap = 60;
    // Optional fuel limit (the adapter passes cambc::fuel_remaining). With fuel_budget > 0 the
    // simulation stops searching at 60% of the budget and stops entirely (unresolved) at 100%.
    std::uint64_t (*fuel_left)() = nullptr;
    std::uint64_t fuel_budget = 0;
    // Debugging only: called after every turn with the state (fast-forward is off when set).
    void (*on_turn)(int turn, bool ours, const Fighter& us, const Fighter& enemy, int distance) = nullptr;
};

struct FightResult {
    bool we_win = false;
    float our_health_worst_case = 0;
    float enemy_health_at_end = 0;
    int turns = 0;            // turns simulated (ours and theirs)
    bool resolved = false;   // false: round cap reached or no fight possible
};

inline bool same_fighter(const Fighter& a, const Fighter& b) {
    return a.design == b.design && a.health == b.health && a.max_health == b.max_health && a.strength == b.strength
        && a.focus == b.focus && a.shield == b.shield && a.shield_turns == b.shield_turns && a.cooldown[0] == b.cooldown[0]
        && a.cooldown[1] == b.cooldown[1] && a.stunned_rounds == b.stunned_rounds && a.damage_taken_mult == b.damage_taken_mult;
}
// Identical model inputs (for reusing a cached prediction).
inline bool same_inputs(const DuelSetup& a, const DuelSetup& b) {
    return same_fighter(a.us, b.us) && same_fighter(a.enemy, b.enemy) && a.walk_distance == b.walk_distance
        && a.enemy_acted_this_round == b.enemy_acted_this_round && a.same_turn_position == b.same_turn_position
        && a.our_turn_now == b.our_turn_now && a.camp_damage_to_us_per_round == b.camp_damage_to_us_per_round
        && a.camp_damage_to_enemy_per_round == b.camp_damage_to_enemy_per_round;
}

