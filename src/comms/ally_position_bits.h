#pragma once
// Bit layout of a teammate position report (comms/ally_positions.h). No SDK: host tests use it.
// Bits 0-5 x / 2, bits 6-11 y / 2 (2-tile resolution, maps up to 127 tiles), bits 12-14 roster
// index, bit 15 set when at or above half health.
#include <algorithm>
#include <cstdint>

inline std::uint16_t pack_ally_position(int x, int y, int roster_index, bool healthy) {
    const auto cx = static_cast<std::uint16_t>(std::clamp(x / 2, 0, 63));
    const auto cy = static_cast<std::uint16_t>(std::clamp(y / 2, 0, 63));
    const auto idx = static_cast<std::uint16_t>(std::clamp(roster_index, 0, 7));
    return static_cast<std::uint16_t>(cx | (cy << 6) | (idx << 12) | (healthy ? 0x8000 : 0));
}

// x and y come back as the lower tile of the 2-tile cell.
inline void unpack_ally_position(std::uint16_t bits, int& x, int& y, int& roster_index, bool& healthy) {
    x = (bits & 63) * 2;
    y = ((bits >> 6) & 63) * 2;
    roster_index = (bits >> 12) & 7;
    healthy = (bits & 0x8000) != 0;
}
