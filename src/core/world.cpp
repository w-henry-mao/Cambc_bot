#include "world.h"
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
    }
}
int altar_walk(const MatchData& match, cambc::vec2 tile) { return match.altar_distance.get(tile); }
