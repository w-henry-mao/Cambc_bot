#include "dash_move.h"
#include <vector>

namespace {
int nearest(cambc::vec2 p, const std::vector<cambc::vec2>& goals) {
    int best = 1 << 20;
    for (const auto g : goals) best = std::min(best, static_cast<int>(p.chebyshev_distance(g)));
    return best;
}
}  // namespace

bool dash_towards(const cambc::bitboard& goals) {
    auto& me = cambc::self();
    if (!me.can_cast() || goals.is_empty()) return false;
    try {
        for (const auto& ab : me.abilities()) {
            if (!ab.is_dash() || !ab.is_ready()) continue;
            const auto targets = goals.positions();
            const int now = nearest(me.position(), targets);
            if (now == 0) return false;
            cambc::vec2 best_tile = me.position();
            int best = now - 1;   // what one ordinary step could reach at best
            for (const auto tile : ab.valid_tiles().positions()) {
                const int d = nearest(tile, targets);
                if (d < best) { best = d; best_tile = tile; }
            }
            if (best_tile == me.position()) return false;
            return ab.use(best_tile);
        }
    } catch (const cambc::error&) {}
    return false;
}
