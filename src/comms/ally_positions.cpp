#include "ally_positions.h"

void share_ally_positions() {
}

const AllyPosition& ally_position(int roster_index) {
    static AllyPosition empty;
    return empty;
}
