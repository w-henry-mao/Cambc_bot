#pragma once
#include <cambc.h>
#include "../constants/test_hooks.h"
// Spend all gold on bruiser focus shards (focus powers the abilities we cast). A dead champion
// has no position, so check death before asking whether we stand on spawn.
inline bool shop_pre_step() {
    auto& me = cambc::self();
    const bool is_dead = !me.is_alive();
    if (!is_dead && !cambc::map::spawn_region(cambc::team::ally).is_set(me.position())
        && me.passive() != cambc::passive_type::remote_shop)
        return false;
    if constexpr (TEST_ALTAR_LOCKER) {   // test opponent only: an altar lock before shards
        const auto lock = cambc::consumable_type::altar_lock;
        if (me.can_buy(lock)) me.buy(lock);
    }
    const auto shard = cambc::item_type::bruiser_shard_of_focus;
    const unsigned count = me.gold_count() / shard.cost();
    if (!count || !me.can_buy(shard, count)) return false;
    me.buy(shard, count);
    return true;
}
