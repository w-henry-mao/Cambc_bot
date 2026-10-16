#pragma once
#include <cambc.h>
#include <vector>
// Every champion in the match, allies first. Champion views query live state through their
// handle, so building them once is enough; building them per call costs ~1.5k fuel each.
const std::vector<cambc::champion>& roster_allies();
const std::vector<cambc::champion>& roster_enemies();
// Alive and visible this turn.
bool in_view(const cambc::champion& c);
// A champion view also caches its ability metadata when it is built, and one built while the
// champion was out of vision rethrows "not in vision" on every ability query. This returns a
// view built the first time `c` was seen, for ability queries. Call only while `c` is visible.
const cambc::champion& ability_view(const cambc::champion& c);
