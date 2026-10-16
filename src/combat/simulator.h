#pragma once
// Full duel simulator: our champion (playing the way our bot fights, see our_policy.h)
// against one enemy playing for maximum aggression (whatever leaves us with the least
// health). Rules follow docs/combat-rules.md. Pure C++: no SDK calls.
#include "model.h"

// Cap on enemy decision branches explored; beyond it the enemy plays its greedy plan.
inline constexpr int kMaxSearchLeaves = 4096;

FightResult simulate_duel(const DuelSetup& setup, const FightOptions& options);

// One enemy turn, as the model plays it. Each plan spends at most one cast.
enum class PlanKind { Plain, Offensive, Reset, Dash, Shield };
struct Plan { PlanKind kind; int slot; };
// The greedy max-aggression plan for the enemy's next turn (a ready stun is used at once):
// what the fast version assumes, and what the test aggressor bot plays.
Plan choose_enemy_plan(const DuelSetup& setup);
// Same model with a search budget of `max_leaves` enemy branches (0 = greedy enemy only).
FightResult simulate_duel_budget(const DuelSetup& setup, const FightOptions& options, int max_leaves);
FightResult simulate_duel_fast(const DuelSetup& setup, const FightOptions& options);
