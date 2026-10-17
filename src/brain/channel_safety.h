#pragma once
#include <cambc.h>
#include "../core/match_data.h"
// Whether a recall or tribute in progress should keep going this turn. Recall is already
// cancelled by the engine on damage or stun; tribute is not stopped by damage, only by stun,
// death or a new altar lock. Teleports cannot be cancelled, so they are never asked about.
//
// Recalls are left alone (damage cancels them anyway). Tributes follow constants::kTributeSafetyPlan:
//   0 never cancel; 1 cancel when hit since our last turn; 2 cancel when the visible enemies
//   already in reach could deal our remaining health before the tribute completes. After a
//   cancel the champion acts normally; with an enemy in reach the attack state outranks tribute.
bool channel_still_safe(const MatchData& match);
