#include "ally_positions.h"
#include <algorithm>
#include <array>
#include <span>
#include "../constants/combat_constants.h"
#include "../core/roster.h"

namespace {
namespace bc = cambc;
constexpr int kMaxAllies = 8;
AllyPosition reports[kMaxAllies];
}  // namespace

void share_ally_positions() {
    const auto& allies = roster_allies();
    const int n = static_cast<int>(allies.size());
    if (n <= 1) return;
    const auto round = bc::round_number();
    int mine = 0;
    for (int i = 0; i < n; ++i)
        if (allies[i] == bc::self()) mine = i;
    // Read: the slot holds this round's report if its writer already acted, else last round's.
    // Anything else is left over from a writer that has not written since (dead, say): ignored.
    std::array<std::byte, 2> bytes{};
    bc::shared_memory.read(constants::kAllyPositionByte, std::span{bytes});
    const auto bits = static_cast<std::uint16_t>(std::to_integer<unsigned>(bytes[0]) | (std::to_integer<unsigned>(bytes[1]) << 8));
    if (bits != 0) {
        int x, y, idx;
        bool healthy;
        unpack_ally_position(bits, x, y, idx, healthy);
        const int now = static_cast<int>(round % static_cast<std::uint32_t>(n));
        const int before = (now + n - 1) % n;
        if (idx != mine && idx < n && idx < kMaxAllies && (idx == now || idx == before)) {
            auto& r = reports[idx];
            r.pos = bc::vec2{x + 1, y + 1};   // middle of the 2-tile cell
            r.heard_round = idx == now ? round : round - 1;
            r.healthy = healthy;
            r.known = true;
        }
    }
    // Write ours on our round.
    if (static_cast<int>(round % static_cast<std::uint32_t>(n)) != mine) return;
    auto& me = bc::self();
    const auto p = me.position();
    const auto out = pack_ally_position(p.x, p.y, mine, me.health() >= 0.5f * me.max_health());
    const std::array<std::byte, 2> w{std::byte(static_cast<unsigned char>(out & 0xff)), std::byte(static_cast<unsigned char>(out >> 8))};
    bc::shared_memory.write(constants::kAllyPositionByte, std::span{w});
}

const AllyPosition& ally_position(int roster_index) {
    static const AllyPosition none{};
    return roster_index >= 0 && roster_index < kMaxAllies ? reports[roster_index] : none;
}
