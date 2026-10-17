#pragma once
// Safe recall: before recalling, get out of reach of what can hit us (damage cancels a recall).
// Danger: tiles within reach of visible enemy champions (attack/spell reach + kRecallEnemyBuffer)
// and of awake camps (attack range + kRecallCampBuffer). Among safe tiles within
// kRecallSearchSteps steps, the best by spot_score (nav/safe_spot_score.h) is the one to walk to.
#include <cambc.h>
#include <optional>
#include "../core/match_data.h"

struct RecallPlan {
    bool safe_here = false;             // nothing we can see can hit our tile
    std::optional<cambc::vec2> spot;    // otherwise: the safe tile to walk to (none in range: nullopt)
    int threats = 0;                    // visible enemy champions and awake camps counted
    cambc::bitboard danger;             // the threatened tiles
};
RecallPlan plan_safe_recall(const MatchData& match);

// Test builds (RECALL_TRACE): log each retreat (recall state chosen while out of spawn) and how
// it ended: home, died, recalls cancelled on the way. Call at the end of every turn.
void recall_trace_note_start(bool used_spot, int threats);
void recall_trace_turn();
