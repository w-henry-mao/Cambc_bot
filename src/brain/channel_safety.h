#pragma once
#include <cambc.h>
#include "../core/match_data.h"
// Whether a recall or tribute in progress should keep going this turn. Recall is already
// cancelled by the engine on damage or stun; tribute is not stopped by damage, only by stun,
// death or a new altar lock. Teleports cannot be cancelled, so they are never asked about.
//
// NOT FINISHED: the real rule needs the fight calculator (task 8), because a channel should
// only be abandoned when a visible enemy can reach and hit us before it completes AND would
// win that fight. Planned shape once task 8 exists:
//   - horizon: turns left in the channel (remaining_channel_time()), plus a tunable allowance
//     for the alternating turn order letting an enemy act twice between our turns
//   - reach: walking distance to tiles from which each visible enemy's basic attack or spells
//     can hit us (melee walks and attacks in one turn; ranged and walk-excluding spells need
//     an extra turn; a ready dash adds steps)
//   - separate rules for recall and tribute, with tribute needing far more danger to cancel
//     (exact tribute rule still to be decided by the team)
// Until then this always returns true, so behaviour is unchanged.
bool channel_still_safe(const MatchData& match);
