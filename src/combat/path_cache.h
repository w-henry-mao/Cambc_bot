#pragma once
// Reusing a fast prediction on a later turn: if the fight went exactly as predicted, the current
// state is one of the recorded path points and the stored answer still holds (fewer turns left).
#include <optional>
#include <vector>
#include "model.h"

struct StoredPath {
    std::vector<PathPoint> points;
    FightResult result;
};

inline std::optional<FightResult> reuse_from_path(const StoredPath& stored, const DuelSetup& now) {
    if (!stored.result.resolved) return std::nullopt;
    // Healths only fall along a path (no healing in a duel) and points are in turn order, so
    // skip points above the current healths and stop at the first one below.
    for (const auto& p : stored.points) {
        if (p.setup.us.health < now.us.health || p.setup.enemy.health < now.enemy.health) break;
        if (p.setup.us.health > now.us.health || p.setup.enemy.health > now.enemy.health) continue;
        auto key = p.setup;
        // Without a shared turn position the order is strict alternation whatever the flag says.
        if (!key.same_turn_position && !now.same_turn_position) key.enemy_acted_this_round = now.enemy_acted_this_round;
        if (!same_inputs(key, now)) continue;
        auto r = stored.result;
        r.turns -= p.turn;
        return r;
    }
    return std::nullopt;
}
