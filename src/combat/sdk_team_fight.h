#pragma once
// Team fights from live SDK state: the rough estimate in team_fight.h, plus a shared-memory
// call so one champion can tell the rest of the team to take a fight.
//
// Vision is not shared: a champion reads only allies it can see itself (allies out of its vision
// throw on position/health). So the estimate counts us, the allies we can see and the enemies we
// can see within kTeamFightRadius. Allies farther away are left out (they are too far to join in
// time anyway); unseen enemies are unknown.
#include <cambc.h>
#include <optional>
#include <vector>
#include "team_fight.h"

struct TeamFightCall {
    TeamEstimate estimate;
    const cambc::champion* target = nullptr;   // the enemy with the least health (in roster_enemies()); null if none
    int allies = 0, enemies = 0;              // champions in the estimate (allies include us)
};

// Estimate the fight around us with the version kTeamFightQuick picks. With no visible enemy in
// the radius: Unsure, no target.
TeamFightCall estimate_team_fight_here();
// A teammate we cannot see, from its shared-memory position report (comms/ally_positions.h).
struct SharedAlly {
    Fighter f;                          // stats from when we last saw it (or base stats), health from the report
    int distance[kTeamFightMaxSide];    // to each enemy in the setup
    int to_me = 0, age = 0;             // tiles from us; rounds since the report
    bool included = false;              // counted in the setup
};
// Every 10 rounds: note the stats of visible teammates not seen for 50 rounds (used for
// teammates the estimate knows only from position reports). Call once per turn.
void remember_teammates();
// The live input estimate_team_fight_here uses: us and visible allies first, then (with
// kTeamFightSharedAllies) unseen teammates reported within kTeamFightSharedRadius. `enemies_out`,
// if given, receives the enemies in setup order (pointers into roster_enemies(); copying champion
// views costs fuel). `shared_out` (test logging) receives every reported teammate within 16 tiles.
TeamFightSetup team_fight_setup_here(const cambc::champion** enemies_out = nullptr, std::vector<SharedAlly>* shared_out = nullptr,
                                     int* visible_count = nullptr);

// A fight call in the shared header word (comms/shared_memory.h): which enemy, when, and where it
// was seen, so teammates that cannot see it know where to go.
struct TeamFightOrder {
    const cambc::champion* target;   // in roster_enemies()
    cambc::vec2 last_seen;
};
// Tell the team to fight `target` (call this while `target` is visible; call again each turn to
// keep the call fresh and its position current).
void call_team_fight(const cambc::champion& target);
// Clear the call (e.g. when the fight is over).
void clear_team_fight_call();
// The live call: written within the last kTeamFightCallRounds rounds and its target still alive.
std::optional<TeamFightOrder> team_fight_called();

// Test builds only (kTeamFightTrace): log estimates at first contact and the deaths that
// followed. Call once at the end of every turn.
void team_fight_trace_turn();
