#pragma once
#include <algorithm>
#include <cstdint>
// Simulation fuel a prediction may spend: never more than what is left after `reserve` (so the
// turn cannot run out mid-prediction). Fast: at most `fast_cap`. Full: everything above reserve.
inline std::uint64_t prediction_budget(bool full, std::uint64_t fuel_left, std::uint64_t fast_cap, std::uint64_t reserve) {
    const std::uint64_t spare = fuel_left > reserve ? fuel_left - reserve : 0;
    return full ? spare : std::min(fast_cap, spare);
}
