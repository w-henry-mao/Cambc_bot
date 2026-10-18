#include "world.h"
#include "../constants/state_constants.h"
void update_world(MatchData& match) {
    auto& me = cambc::self();
    const float health = me.health();
    match.hit_since_last_turn = match.last_health >= 0 && health < match.last_health;
    match.last_health = health;
    // Respawn timers and uncaptured rewards of the camp groups in sight; one host call per group
    // to test its centre tile, then a state query only for the visible ones.
    const auto visible = cambc::vision::visible_tiles();
    const auto round = cambc::round_number();
    for (auto& group : match.camp_groups) {
        if (!group.view || !visible.is_set(group.centre)) continue;
        const auto state = group.view->state();
        group.reward_waiting = std::holds_alternative<cambc::map::camp_group_reward_available>(state);
        if (const auto* respawning = std::get_if<cambc::map::camp_group_respawning>(&state))
            group.respawn_ready_round = round + static_cast<std::uint32_t>(respawning->timer);
        else
            group.respawn_ready_round = 0;
        // Left to a teammate: live state replaces our guess once the group is captured or back. A
        // reward still waiting well after the capture should have finished was never collected.
        if (group.left_to_teammate_round
            && (!group.reward_waiting
                || round > group.left_to_teammate_round + static_cast<std::uint32_t>(cambc::constants::capture_time) + constants::kCollectorGraceRounds))
            group.left_to_teammate_round = 0;
    }
}
int altar_walk(const MatchData& match, cambc::vec2 tile) { return match.altar_distance.get(tile); }
