#pragma once
#include <cambc.h>
#include <algorithm>
#include "../constants/test_hooks.h"
// Spend all gold on Bruiser health shards. Tested against Bruiser focus (the old default) and
// six other plans (README, Shop): health won 109/136 vs focus 89/136 on fresh opponents. A dead
// champion has no position, so check death before asking whether we stand on spawn.
inline bool shop_pre_step() {
    auto& me = cambc::self();
    const bool is_dead = !me.is_alive();
    if (!is_dead && !cambc::map::spawn_region(cambc::team::ally).is_set(me.position())
        && me.passive() != cambc::passive_type::remote_shop)
        return false;
    if constexpr (TEST_ALTAR_LOCKER || ALTAR_LOCK_PLAN != 0) {   // an altar lock (carry one) before shards
        const auto lock = cambc::consumable_type::altar_lock;
        if (me.can_buy(lock)) me.buy(lock);
    }
    // Test builds may change the plan (SHOP_PLAN): first complete one emblem of `first` (its focus
    // variant), then spend the rest on `main`. Plan 0 (normal builds): Bruiser health shards only.
    using it = cambc::item_type;
    it first = it::bruiser_shard_of_health, first_emblem = it::bruiser_emblem_of_health, main = it::bruiser_shard_of_health;
    switch (SHOP_PLAN) {
        case 1: first = it::home_shard_of_focus; first_emblem = it::home_emblem_of_focus; break;
        case 2: first = it::durability_shard_of_focus; first_emblem = it::durability_emblem_of_focus; break;
        case 3: first = it::jungle_shard_of_focus; first_emblem = it::jungle_emblem_of_focus; break;
        case 4: first = it::monster_shard_of_focus; first_emblem = it::monster_emblem_of_focus; break;
        case 5: main = it::power_shard_of_focus; first = main; first_emblem = it::power_emblem_of_focus; break;
        case 6: main = it::bruiser_shard_of_strength; first = main; first_emblem = it::bruiser_emblem_of_strength; break;
        case 7: break;   // Bruiser health (now plan 0)
        case 8: main = it::bruiser_shard_of_focus; first = main; first_emblem = it::bruiser_emblem_of_focus; break;   // old default
        default: break;
    }
    bool bought = false;
    if (first != main) {
        const auto items = me.item_count();
        if (items[first_emblem] == 0) {
            const unsigned need = 5 - std::min(4u, static_cast<unsigned>(items[first]));
            const unsigned n = std::min(need, me.gold_count() / first.cost());
            if (n && me.can_buy(first, n)) { me.buy(first, n); bought = true; }
            if (n < need) return bought;   // finish the emblem before anything else
        }
    }
    const unsigned count = me.gold_count() / main.cost();
    if (!count || !me.can_buy(main, count)) return bought;
    me.buy(main, count);
    return true;
}
