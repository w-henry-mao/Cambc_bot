#include "turn_order.h"
std::vector<bool> turn_sequence(bool enemy_acted_this_round, int count) {
    std::vector<bool> seq;
    for (int t = 0; t < count; ++t) seq.push_back(our_turn_at(enemy_acted_this_round, t));
    return seq;
}
