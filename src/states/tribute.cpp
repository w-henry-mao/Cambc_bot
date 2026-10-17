#include <cambc.h>
#include <iostream>
#include "../constants/test_hooks.h"
#include "tribute.h"
#include "../core/context.h"
#include "../core/geometry.h"
#include "../core/match_data.h"
#include "../core/roster.h"
#include "../core/world.h"
#include "../nav/blocked_tiles.h"
#include "../nav/pathfinding.h"
#include "../constants/brain_constants.h"
#include "../constants/state_constants.h"
namespace {
// Gems we could bank right now: ours plus every visible ally's.
std::uint32_t team_gems_carried() {
    std::uint32_t gems = cambc::self().gem_count();
    for (const auto& ally : roster_allies())
        if (!(ally == cambc::self()) && in_view(ally)) gems += ally.gem_count();
    return gems;
}
}
// Bank gems when we carry enough, when an altar is close, or when the gems on hand would
// take the team past the winning total.
int TributeState::score(const TurnContext& ctx) {
    auto& me = cambc::self();
    const auto gems = me.gem_count();
    if (gems == 0) return 0;
    if (gems >= constants::kTributeGems) return constants::kTributeScore;
    if (altar_walk(*ctx.match_data, me.position()) <= constants::kTributeIfAltarWithin) return constants::kTributeScore;
    if (cambc::global_submitted_gems(cambc::team::ally) + team_gems_carried() >= cambc::constants::winning_gems) return constants::kTributeScore;
    return 0;
}
StateResult TributeState::execute(TurnContext& ctx, BotMemory& memory) {
    auto& me = cambc::self();
    // An enemy altar lock blocks our tributes at that altar until three basic attacks break it;
    // our own locks do not block us.
    const auto enemy_locks = cambc::vision::altar_locks(cambc::team::enemy);
    auto enemy_locked = [&](vec2 altar) {
        if constexpr (!BREAK_ALTAR_LOCKS) return cambc::vision::has_altar_lock(altar);   // old behaviour (tests)
        for (const auto& lock : enemy_locks)
            if (lock.position() == altar) return true;
        return false;
    };
    for (auto altar : cambc::map::altars().positions()) {
        if (distance(me.position(), altar) <= cambc::constants::submit_gems_range && !enemy_locked(altar)) {
            me.submit_gems(altar);
            return StateResult::Done;
        }
    }
    // At a locked altar: break the lock, then tribute once it is gone.
    if (BREAK_ALTAR_LOCKS && me.can_attack()) {
        const auto attack_tiles = me.attack_tiles();
        for (const auto& lock : enemy_locks)
            if (distance(me.position(), lock.position()) <= cambc::constants::submit_gems_range && attack_tiles.is_set(lock.position())) {
                me.attack(lock);
                if constexpr (SCORE_TRACE) std::cout << "LOCK_HIT r=" << cambc::round_number() << '\n';
                return StateResult::Continue;
            }
    }
    if (!me.can_move()) return StateResult::Continue;
    // Walk down the precomputed altar distance (terrain only, so cheap). If units or camps block
    // every step that gets closer, path around them with a fresh search instead.
    const auto& match = *ctx.match_data;
    const auto blocked = blocked_tiles();
    auto step = me.position();
    int best = altar_walk(match, step);
    for (const auto tile : blocked.reachable_tiles(step, me.base_stats().base_move_speed).positions()) {
        const int d = altar_walk(match, tile);
        if (d < best) { best = d; step = tile; }
    }
    if (step != me.position()) {
        me.move_absolute(step);
        return StateResult::Continue;
    }
    cambc::bitboard approach;
    approach.clear();
    // Locked altars count too: we break enemy locks when we get there.
    for (const auto altar : cambc::map::altars().positions())
        if (BREAK_ALTAR_LOCKS || !cambc::vision::has_altar_lock(altar)) approach |= tiles_around(altar, cambc::constants::submit_gems_range);
    move_towards_closest(approach);
    return StateResult::Continue;
}
TributeState tribute_state_instance;
