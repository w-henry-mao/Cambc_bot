#include "team_fight_call.h"
#include "../combat/sdk_team_fight.h"
#include <iostream>
#include <cambc.h>
#include "../constants/combat_constants.h"
#include "../core/roster.h"

// The team's rule for calling a fight goes here. The pieces available:
//   estimate_team_fight_here()  -> TeamFightCall: .estimate.call (Fight / Avoid / Unsure),
//                                  .target (the enemy with the least health), .allies, .enemies
//   call_team_fight(target)     -> every teammate goes for `target` (repeat each turn to keep the
//                                  call alive; it lapses after kTeamFightCallRounds rounds)
//   clear_team_fight_call()     -> cancel
//   team_fight_called()         -> the live call, if any
// Left empty on purpose: no champion calls a fight yet.
void decide_team_fight_call() {
    if constexpr (constants::kTeamFightTestCaller) {
        // Test builds only: re-estimate every kTeamFightTestCallEvery rounds (champions staggered by
        // roster index) and call whenever the estimate says Fight, to measure the follow-up.
        const auto every = static_cast<std::uint32_t>(constants::kTeamFightTestCallEvery);
        if (every > 1) {
            const auto& allies = roster_allies();
            std::uint32_t mine = 0;
            for (std::uint32_t i = 0; i < allies.size(); ++i)
                if (allies[i] == cambc::self()) mine = i;
            if (cambc::round_number() % every != mine % every) return;
        }
        const auto call = estimate_team_fight_here();
        if (call.estimate.call == TeamCall::Fight && call.target) {
            call_team_fight(*call.target);
            std::cout << "TEAM_CALL r=" << cambc::round_number() << " me=" << cambc::to_string(cambc::self().champion_name())
                      << " target=" << cambc::to_string(call.target->champion_name()) << " allies=" << call.allies
                      << " enemies=" << call.enemies << '\n';
        }
    }
}
