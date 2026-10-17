#pragma once
// How our bot fights, as pure decisions shared by the live bot (states/attack.cpp,
// skills/casting.cpp) and the fight simulator, so the two cannot drift apart.
#include "model.h"

// Whether our attack state walks toward an enemy it cannot hit yet (states/attack.cpp: it walks
// toward the tiles within attack range of the target). Keep in step with attack.cpp; the duel
// tests (tools/duel_harness.py) check they agree.
inline constexpr bool kAttackStateClosesIn = true;

// Support casts (heal/shield) fire when the target is below this fraction of max health.
inline constexpr float kSupportBelow = 0.8f;

struct PolicyView {
    const ChampionDesign* design = nullptr;
    int distance = 0;                 // Chebyshev distance to the target
    bool can_move = true, can_attack = true, can_cast = true;
    int cooldown[2] = {0, 0};         // 0 = ready
    float health_fraction = 1;        // our health / max health
};

bool is_offensive(const AbilityDesign& a);
bool is_support(const AbilityDesign& a);
// First ready offensive slot (damage, area damage, crowd control) that targets enemies and
// reaches the target, in slot order; -1 if none or we cannot cast.
int offensive_slot(const PolicyView& v);
// First ready heal/shield slot we may cast on ourselves while below kSupportBelow; -1 if none.
int support_slot_self(const PolicyView& v);
// First ready attack-reset slot; -1 if none. (Only usable after the attack has been spent.)
int reset_slot(const PolicyView& v);
// How far our bot engages from: the longest of attack range and damage/area/stun spell
// ranges (states/attack.cpp only fights enemies this close, otherwise it keeps farming).
int reach(const ChampionDesign& d);
// How far `d` can hit from where it stands: the longest of attack range and damage/area/stun
// spell ranges (reach() without the test-build bonus).
int threat_reach(const ChampionDesign& d);
// Basic attack possible now: allowance available and target within attack range.
bool can_hit(const PolicyView& v);
