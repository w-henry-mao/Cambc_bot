#pragma once
// How many gems an enemy champion carries, and what it stands to gain from the camp it is
// fighting. Carried gems of a visible enemy are exact (champion::gem_count works on enemies; a
// probe matched 76/76 readings against the enemy's own count). Out of sight we only know the
// last reading. Nothing calls this yet: it is for the team's fight and pursuit rules.
#include <cambc.h>
#include <cstdint>
#include "match_data.h"

struct EnemyGems {
    bool known = false;               // seen at least once
    std::uint32_t carried = 0;        // exact when visible now, else the last reading
    std::uint32_t rounds_old = 0;     // 0 when visible now
    std::uint32_t camp_gems = 0;      // gem value of the camp group it is fighting (damaged camp in its reach)
    std::uint32_t total() const { return carried + camp_gems; }   // what it holds once it captures that group
};

// Reads live state when `enemy` is visible (about one host call, plus a scan of visible camps).
EnemyGems enemy_gems(const cambc::champion& enemy, const MatchData& match);
