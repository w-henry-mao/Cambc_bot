#include "safe_recall.h"
#include <iostream>
#include <vector>
#include "blocked_tiles.h"
#include "pathfinding.h"
#include "safe_spot_score.h"
#include "../combat/design_table.h"
#include "../combat/our_policy.h"
#include "../constants/combat_constants.h"
#include "../constants/state_constants.h"
#include "../constants/test_hooks.h"
#include "../core/geometry.h"
#include "../core/roster.h"

namespace {
namespace bc = cambc;

// A camp group wakes when damaged and its camps attack anyone in range; we cannot read the group
// state, so a camp counts as awake when it or a camp next to it (same group) is damaged.
constexpr int kCampGroupSpread = 3;
}  // namespace

RecallPlan plan_safe_recall(const MatchData& match) {
    RecallPlan plan;
    plan.danger.clear();
    auto& me = bc::self();
    const auto here = me.position();
    std::vector<bc::vec2> enemies;
    for (const auto& e : roster_enemies()) {
        if (!in_view(e)) continue;
        const auto p = e.position();
        const auto& d = design_for(static_cast<int>(e.champion_name()));
        plan.danger.draw_chebyshev(p, static_cast<float>(threat_reach(d) + constants::kRecallEnemyBuffer));
        enemies.push_back(p);
        ++plan.threats;
    }
    const auto camps = bc::vision::camps();
    std::vector<bc::vec2> damaged;
    for (const auto& c : camps)
        if (c.health() > 0 && c.health() < c.max_health()) damaged.push_back(c.position());
    if (!damaged.empty())
        for (const auto& c : camps) {
            if (c.health() <= 0) continue;
            const auto p = c.position();
            bool awake = false;
            for (const auto& q : damaged) awake |= p.chebyshev_distance(q) <= kCampGroupSpread;
            if (!awake) continue;
            plan.danger.draw_chebyshev(p, static_cast<float>(constants::kCampAttackRange + constants::kRecallCampBuffer));
            ++plan.threats;
        }
    if (!plan.danger.is_set(here)) { plan.safe_here = true; return plan; }

    // Safe tiles we can walk to within kRecallSearchSteps.
    const auto blocked = blocked_tiles();
    bc::distance_field field;
    bc::bitboard visited;
    field.compute(blocked, tiles_around(here, 0), visited, static_cast<std::uint16_t>(constants::kRecallSearchSteps));
    auto candidates = visited & ~plan.danger & ~blocked;
    if (candidates.is_empty()) return plan;

    // Tiebreak inputs: visible teammates, and visible enemies other than the closest one.
    int fx[8], fy[8], nf = 0, ex[8], ey[8], ne = 0;
    for (const auto& a : roster_allies())
        if (nf < 8 && a != me && in_view(a)) { const auto p = a.position(); fx[nf] = p.x; fy[nf++] = p.y; }
    int closest = -1;
    for (int i = 0; i < static_cast<int>(enemies.size()); ++i)
        if (closest < 0 || here.chebyshev_distance(enemies[i]) < here.chebyshev_distance(enemies[closest])) closest = i;
    for (int i = 0; i < static_cast<int>(enemies.size()) && ne < 8; ++i)
        if (i != closest) { ex[ne] = enemies[i].x; ey[ne++] = enemies[i].y; }
    const SpotWeights w{constants::kRecallWeightSteps, constants::kRecallWeightHome, constants::kRecallWeightFriends,
                        constants::kRecallWeightEnemies, constants::kRecallBushBonus, constants::kRecallEnemyDistCap};

    // Per-tile reads from plain arrays: distance_field::get and bitboard::is_set are host calls
    // (~100 fuel each). The step field is exported once into a reused buffer.
    static std::vector<std::uint16_t> steps;
    steps.resize(bc::distance_field::linear_buffer_size() / sizeof(std::uint16_t));
    field.export_linear_buffer(steps);
    const std::size_t width = static_cast<std::size_t>(match.map_width);
    const bool have_static = width > 0 && !match.home_walk.empty();
    // Scored: every safe bush tile, and plain safe tiles on a 2x2 grid (neighbouring tiles score
    // almost alike; each scored tile costs ~300 fuel).
    const auto tiles = (candidates & (match.sample_grid | bc::map::bushes())).positions();
    if (tiles.empty()) { plan.spot = candidates.positions().front(); return plan; }
    // Pass 1: the nearest safe tile. Pass 2: score the ones within kRecallExtraSteps of it.
    int nearest = 1 << 20;
    for (const auto tile : tiles) nearest = std::min<int>(nearest, steps[static_cast<std::size_t>(tile.y) * width + static_cast<std::size_t>(tile.x)]);
    const int limit = nearest + constants::kRecallExtraSteps;
    auto cheb = [](int ax, int ay, int bx, int by) { const int dx = ax > bx ? ax - bx : bx - ax, dy = ay > by ? ay - by : by - ay; return dx > dy ? dx : dy; };
    float best = 0;
    for (const auto tile : tiles) {
        const auto idx = static_cast<std::size_t>(tile.y) * width + static_cast<std::size_t>(tile.x);
        SpotFeatures f;
        f.steps = steps[idx];
        if (f.steps > limit) continue;
        const auto home = have_static ? match.home_walk[idx] : bc::distance_field::unreachable;
        f.home = home == bc::distance_field::unreachable ? 200 : home;
        for (int i = 0; i < nf; ++i) { const int d = cheb(tile.x, tile.y, fx[i], fy[i]); if (f.friend_dist < 0 || d < f.friend_dist) f.friend_dist = d; }
        for (int i = 0; i < ne; ++i) { const int d = cheb(tile.x, tile.y, ex[i], ey[i]); if (f.enemy_dist < 0 || d < f.enemy_dist) f.enemy_dist = d; }
        f.bush = have_static && match.bush_tiles[idx] != 0;
        const float s = spot_score(f, w);
        if (!plan.spot || s > best) { plan.spot = tile; best = s; }
    }
    return plan;
}

// ---- Test-build trace ----------------------------------------------------------------------
namespace {
struct Episode {
    bool active = false, used_spot = false, was_recalling = false;
    std::uint32_t start = 0;
    float start_hp = 0;
    int threats = 0, cancels = 0;
};
Episode episode;
bool on_home() { return bc::map::spawn_region(bc::team::ally).is_set(bc::self().position()); }
void end_episode(const char* how) {
    auto& me = bc::self();
    std::cout << "RECALL_END r=" << bc::round_number() << " me=" << bc::to_string(me.champion_name()) << " start=" << episode.start
              << " rounds=" << (bc::round_number() - episode.start) << " how=" << how << " start_hp=" << episode.start_hp
              << " used_spot=" << episode.used_spot << " threats=" << episode.threats << " cancels=" << episode.cancels << '\n';
    episode = Episode{};
}
}  // namespace

void recall_trace_note_start(bool used_spot, int threats) {
    if constexpr (!RECALL_TRACE) return;
    else {
        if (episode.active) { episode.used_spot |= used_spot; return; }
        auto& me = bc::self();
        episode = Episode{true, used_spot, false, bc::round_number(), me.max_health() > 0 ? me.health() / me.max_health() : 0, threats, 0};
    }
}

void recall_trace_turn() {
    if constexpr (!RECALL_TRACE) return;
    else {
        try {
            if (!episode.active) return;
            auto& me = bc::self();
            if (!me.is_alive()) { end_episode("died"); return; }
            if (on_home()) { end_episode("home"); return; }
            const bool recalling = me.is_recalling();
            if (episode.was_recalling && !recalling) ++episode.cancels;
            episode.was_recalling = recalling;
            if (bc::round_number() - episode.start > 120) end_episode("gave_up");
        } catch (const std::exception&) {}
    }
}
