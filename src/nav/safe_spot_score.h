#pragma once
// Scoring of candidate tiles for a safe recall (nav/safe_recall.cpp). No SDK: host tests use it.
#include <algorithm>

struct SpotFeatures {
    int steps = 0;           // walk from us to the tile
    int home = 0;            // walk from the tile to our spawn
    int friend_dist = -1;    // tiles to the nearest visible teammate; -1 if none visible
    int enemy_dist = -1;     // tiles to the nearest visible enemy other than the closest one; -1 if none
    bool bush = false;
};
struct SpotWeights {
    float steps, home, friends, enemies, bush;
    int enemy_cap;           // enemy distance counts up to this many tiles
};

// Higher is better: few steps, near home, near teammates, away from other enemies, bushes.
inline float spot_score(const SpotFeatures& f, const SpotWeights& w) {
    float s = -w.steps * static_cast<float>(f.steps) - w.home * static_cast<float>(f.home);
    if (f.friend_dist >= 0) s -= w.friends * static_cast<float>(f.friend_dist);
    if (f.enemy_dist >= 0) s += w.enemies * static_cast<float>(std::min(f.enemy_dist, w.enemy_cap));
    if (f.bush) s += w.bush;
    return s;
}
