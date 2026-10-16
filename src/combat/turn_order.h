#pragma once
#include <vector>
// Order of turns in a duel, starting with our current turn (we are deciding right now).
// The starting team swaps every round, so one side sometimes acts twice in a row.
// true = our turn, false = the enemy's.
std::vector<bool> turn_sequence(bool enemy_acted_this_round, int count);
// The same order without building a list: is turn `t` (0 = now) ours?
// Turns alternate team by team, champion by champion, and the starting team swaps each round.
// Only when both fighters hold the same position in their team's order does one side get two
// turns in a row; otherwise the order from now is strictly us, them, us, them.
// When predicting for an ally that is not acting right now, the enemy is given the first turn
// (its true place in the order is not known; moving first is never worse for the enemy).
inline bool our_turn_at(bool enemy_acted_this_round, int t, bool same_position = true, bool our_turn_now = true) {
    if (!our_turn_now) { if (t == 0) return false; --t; }
    if (!same_position) return t % 2 == 0;
    return (enemy_acted_this_round ? t / 2 : (t + 1) / 2) % 2 == 0;
}
