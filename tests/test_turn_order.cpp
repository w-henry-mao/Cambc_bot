#include "test.h"
#include "combat/turn_order.h"
// The starting team swaps every round (docs/combat-rules.md #8). Index 0 is our current turn.
TEST(test_alternation_when_we_went_first) {
    // Round r: us(now), enemy. Round r+1: enemy, us. Round r+2: us, enemy.
    const auto s = turn_sequence(/*enemy_acted_this_round=*/false, 6);
    const std::vector<bool> expected{true, false, false, true, true, false};
    CHECK(s == expected);
}
TEST(test_double_turn_when_enemy_went_first) {
    // Round r: enemy, us(now). Round r+1: us, enemy. So we act twice in a row.
    const auto s = turn_sequence(/*enemy_acted_this_round=*/true, 6);
    const std::vector<bool> expected{true, true, false, false, true, true};
    CHECK(s == expected);
}
TEST(test_enemy_double_turn_exists) {
    const auto s = turn_sequence(false, 4);
    CHECK(!s[1] && !s[2]);   // enemy acts twice between two of our turns
}
// Turns alternate team by team, champion by champion (A1 B1 A2 B2 ...), and the starting team
// swaps each round. Only fighters at the same position in their team's order get double turns;
// otherwise one of them always goes first and the order from now is strictly us, them, us, them.
TEST(test_strict_alternation_when_positions_differ) {
    for (int t = 0; t < 8; ++t) {
        CHECK(our_turn_at(false, t, /*same_position=*/false) == (t % 2 == 0));
        CHECK(our_turn_at(true, t, /*same_position=*/false) == (t % 2 == 0));
    }
    CHECK(our_turn_at(false, 2, true) == false);   // the swapping pattern still applies at equal positions
    CHECK(our_turn_at(true, 1, true) == true);
}
