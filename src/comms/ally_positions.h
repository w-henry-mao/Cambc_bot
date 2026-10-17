#pragma once
// Teammate positions over 2 bytes of shared memory (bytes 4-5, comms/shared_memory.h), so a
// champion knows roughly where teammates it cannot see are (vision is not shared).
//
// 16 bits hold one champion: bits 0-5 x / 2, bits 6-11 y / 2 (2-tile resolution, maps up to
// 127 tiles), bits 12-14 roster index, bit 15 set when at or above half health. Champions take
// turns: in round r the ally with roster index r % (number of allies) writes, so each teammate is
// heard every few rounds. Readers keep the last report from each teammate.
#include <cambc.h>
#include <cstdint>
#include "ally_position_bits.h"

struct AllyPosition {
    cambc::vec2 pos{};               // to within a tile
    std::uint32_t heard_round = 0;   // the round it was written
    bool healthy = true;             // at or above half health then
    bool known = false;
};

// Once per turn while alive: read the latest report, then write ours if it is our round.
void share_ally_positions();
// The last report from the ally at `roster_index` in roster_allies().
const AllyPosition& ally_position(int roster_index);
