#pragma once
#include <cambc.h>
#include <array>
#include "../constants/state_constants.h"
inline bool shop_pre_step() {
    auto& me = cambc::self();
    const bool at_spawn = cambc::map::spawn_region(cambc::team::ally).is_set(me.position());
    const bool is_dead = !me.is_alive();
    if (!at_spawn && !is_dead) return false;
    const std::array items{cambc::item_type::power_shard_of_strength,
        cambc::item_type::power_shard_of_focus, cambc::item_type::power_shard_of_health};
    const auto item = items[constants::kDefaultShardStat];
    const unsigned count = me.gold_count() / item.cost();
    if (count && me.can_buy(item, count)) {
        me.buy(item, count);
        return true;
    }
    return false;
}
