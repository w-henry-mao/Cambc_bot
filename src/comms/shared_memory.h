#pragma once
// Team shared memory: 8 bytes every champion on our team can read and write during the match.
// This file owns the layout; the packing below has no SDK dependency (host tests use it).
//
// Byte  Use                                               Owner
// 0-3   Header word (32 bits, byte 0 = lowest 8 bits):    comms/team_comms.h
//         bits 0-5    farm target: camp group + 1       states/farm.cpp
//                     (0 = none; up to 63 groups, most on a map today 42)
//         bits 6-8    farm target: round written & 7
//         bits 9-11   fight call: enemy roster index + 1 combat/sdk_team_fight.cpp
//                     (0 = none)
//         bits 12-17  fight call: round written & 63
//         bits 18-24  fight call: x where the target was seen (maps up to 127 tiles)
//         bits 25-31  fight call: y
// 4-5   Teammate position report (below)                  comms/ally_positions.cpp
// 6-7   Free
//
// Rounds are stored modulo 8 / 64. Expired entries are cleared every round by
// sweep_team_header() (comms/team_comms.h), which every champion runs at the start of its turn,
// dead or alive; otherwise an entry nobody rewrote would look fresh again once the counter wraps.
//
// Teammate position report, 16 bits (byte 4 = low 8 bits, byte 5 = high 8 bits):
//   bits 0-5    x / 2       2-tile resolution; maps up to 127 tiles wide (largest today 100)
//   bits 6-11   y / 2
//   bits 12-14  roster index of the teammate reporting (0-7, roster_allies() order)
//   bit  15     1 if that teammate is at or above half health
// One teammate writes per round, in turn: in round r the teammate with roster index
// r % (number of teammates) writes its own report. Every champion reads the slot on its turn and
// keeps the last report from each teammate, so each is heard once every few rounds (every 6
// with 6 bodies). A report is accepted only if its index matches this round's or last round's
// writer, so a dead teammate's stale report is ignored. Packing: comms/ally_position_bits.h.
#include <array>
#include <cstddef>
#include <cstdint>

namespace team_comms {
constexpr std::size_t kHeaderByte = 0;     // 4 bytes
constexpr std::size_t kPositionByte = 4;   // 2 bytes
constexpr std::size_t kFreeByte = 6;       // 2 bytes; used only by the rally test (kBunchMode 1)
constexpr std::size_t kRallyByte = 6;

// Rally point (test feature, kBunchMode 1), bytes 6-7: bits 0-5 x / 2 + 1 (never zero, so an
// all-zero slot means none; maps up to 124 tiles wide), bits 6-11 y / 2, bits 12-15 round & 15.
// Cleared by sweep_team_header() once older than kRallyRounds.
constexpr int kRallyRoundBits = 4;
inline std::uint16_t pack_rally(int x, int y, std::uint32_t round) {
    auto c = [](int v, int max) { return static_cast<std::uint16_t>(v < 0 ? 0 : v / 2 > max ? max : v / 2); };
    return static_cast<std::uint16_t>((c(x, 62) + 1) | (c(y, 63) << 6) | ((round & 15u) << 12));
}
struct RallyField { bool set = false; int x = 0, y = 0, round_low = 0; };
inline RallyField unpack_rally(std::uint16_t bits) {
    if ((bits & 63) == 0) return {};
    return {true, ((bits & 63) - 1) * 2 + 1, ((bits >> 6) & 63) * 2 + 1, static_cast<int>((bits >> 12) & 15)};
}

constexpr int kFarmRoundBits = 3, kCallRoundBits = 6;
constexpr int kFarmTargetRounds = 2;   // a farm target older than this is ignored
constexpr int kMaxFarmGroups = 63;   // group + 1 must fit 6 bits
constexpr int kMaxCoord = 127;       // 7-bit coordinates

struct FarmField { int group_plus1 = 0; int round_low = 0; };
struct CallField { int enemy_plus1 = 0; int round_low = 0; int x = 0; int y = 0; };

inline FarmField get_farm(std::uint32_t w) { return {static_cast<int>(w & 63u), static_cast<int>((w >> 6) & 7u)}; }
inline std::uint32_t set_farm(std::uint32_t w, int group_plus1, std::uint32_t round) {
    const auto g = static_cast<std::uint32_t>(group_plus1 < 0 || group_plus1 > kMaxFarmGroups ? 0 : group_plus1);
    return (w & ~0x1FFu) | g | ((round & 7u) << 6);
}
inline std::uint32_t clear_farm(std::uint32_t w) { return w & ~0x1FFu; }

inline CallField get_call(std::uint32_t w) {
    return {static_cast<int>((w >> 9) & 7u), static_cast<int>((w >> 12) & 63u), static_cast<int>((w >> 18) & 127u),
            static_cast<int>((w >> 25) & 127u)};
}
inline std::uint32_t set_call(std::uint32_t w, int enemy_plus1, std::uint32_t round, int x, int y) {
    auto c7 = [](int v) { return static_cast<std::uint32_t>(v < 0 ? 0 : v > kMaxCoord ? kMaxCoord : v); };
    const auto e = static_cast<std::uint32_t>(enemy_plus1 < 0 || enemy_plus1 > 7 ? 0 : enemy_plus1);
    return (w & 0x1FFu) | (e << 9) | ((round & 63u) << 12) | (c7(x) << 18) | (c7(y) << 25);
}
inline std::uint32_t clear_call(std::uint32_t w) { return w & 0x1FFu; }

// Rounds since an entry was written, from the stored low bits.
inline int age(std::uint32_t round_now, int round_low, int bits) {
    const std::uint32_t mask = (1u << bits) - 1u;
    return static_cast<int>((round_now - static_cast<std::uint32_t>(round_low)) & mask);
}

inline std::uint32_t word_from_bytes(const std::array<std::uint8_t, 4>& b) {
    return static_cast<std::uint32_t>(b[0]) | (static_cast<std::uint32_t>(b[1]) << 8) | (static_cast<std::uint32_t>(b[2]) << 16)
        | (static_cast<std::uint32_t>(b[3]) << 24);
}
inline std::array<std::uint8_t, 4> bytes_from_word(std::uint32_t w) {
    return {static_cast<std::uint8_t>(w & 0xff), static_cast<std::uint8_t>((w >> 8) & 0xff), static_cast<std::uint8_t>((w >> 16) & 0xff),
            static_cast<std::uint8_t>(w >> 24)};
}
}  // namespace team_comms

struct SharedMemory {
    std::array<std::uint8_t, 8> data{};
    std::array<std::uint8_t, 8>& buffer() { return data; }
};
