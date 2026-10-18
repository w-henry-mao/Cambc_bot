#pragma once
#include <cambc.h>
#include <algorithm>
#include "../constants/test_hooks.h"
// Default plan 13 (README, Shop): Power, Durability health and Bruiser health emblems, then Power
// shards of the main stat, like the top two teams. Plan 0 spends everything on Bruiser health
// shards (the previous default). A dead champion has no position, so check death before asking
// whether we stand on spawn.
// Test plans 9+ (top-team builds): finish one emblem per step in order, then spend the rest on
// Power shards of the champion's main stat. Main stat: focus for the spell damage dealers
// (Cassi, Stellar, Prism, and Ren in plans 9-12), strength for the rest.
//   9  Power, Durability health, Bruiser health        (Fish)
//   10 Jungle, Power, Durability, Bruiser health       (Mite; Jungle/Durability in the main stat)
//   11 Power, Durability health                        (Fish without Bruiser)
//   12 Durability health, then Bruiser health shards   (our build plus one emblem)
//   13 plan 9 with Willow on focus (heals); the default
//   14 plan 13 with a Jungle emblem (main stat) first: 30% less damage from camps
struct ShopStep { cambc::item_type shard, emblem; };
struct ShopPlan { ShopStep steps[4]; int n_steps = 0; cambc::item_type main = cambc::item_type::power_shard_of_strength; };
inline ShopPlan shop_plan_for(cambc::champion& me) {
    using it = cambc::item_type;
    using ct = cambc::champion_type;
    const auto type = me.champion_name();
    const bool focus = type == ct::cassi || type == ct::stellar || type == ct::prism || type == ct::ren
        || ((SHOP_PLAN == 13 || SHOP_PLAN == 14) && type == ct::willow);
    using Step = ShopStep;
    const Step power = focus ? Step{it::power_shard_of_focus, it::power_emblem_of_focus}
                             : Step{it::power_shard_of_strength, it::power_emblem_of_strength};
    const Step jungle = focus ? Step{it::jungle_shard_of_focus, it::jungle_emblem_of_focus}
                              : Step{it::jungle_shard_of_strength, it::jungle_emblem_of_strength};
    const Step dur_main = focus ? Step{it::durability_shard_of_focus, it::durability_emblem_of_focus}
                                : Step{it::durability_shard_of_strength, it::durability_emblem_of_strength};
    const Step dur_hp{it::durability_shard_of_health, it::durability_emblem_of_health};
    const Step bru_hp{it::bruiser_shard_of_health, it::bruiser_emblem_of_health};
    ShopPlan plan;
    plan.main = power.shard;
    auto& steps = plan.steps;
    switch (SHOP_PLAN) {
        case 10: steps[0] = jungle; steps[1] = power; steps[2] = dur_main; steps[3] = bru_hp; plan.n_steps = 4; break;
        case 11: steps[0] = power; steps[1] = dur_hp; plan.n_steps = 2; break;
        case 12: steps[0] = dur_hp; plan.n_steps = 1; plan.main = bru_hp.shard; break;
        case 14: steps[0] = jungle; steps[1] = power; steps[2] = dur_hp; steps[3] = bru_hp; plan.n_steps = 4; break;
        default: steps[0] = power; steps[1] = dur_hp; steps[2] = bru_hp; plan.n_steps = 3; break;
    }
    return plan;
}
inline bool shop_emblem_plan(cambc::champion& me) {
    const auto plan = shop_plan_for(me);
    bool bought = false;
    const auto items = me.item_count();
    for (int i = 0; i < plan.n_steps; ++i) {
        const auto& step = plan.steps[i];
        if (items[step.emblem] != 0) continue;
        const unsigned need = 5 - std::min(4u, static_cast<unsigned>(items[step.shard]));
        const unsigned n = std::min(need, me.gold_count() / step.shard.cost());
        if (n && me.can_buy(step.shard, n)) { me.buy(step.shard, n); bought = true; }
        if (n < need) return bought;   // finish this emblem before the next step
    }
    const unsigned count = me.gold_count() / plan.main.cost();
    if (!count || !me.can_buy(plan.main, count)) return bought;
    me.buy(plan.main, count);
    return true;
}
// Gold that completes the next emblem in our buy order (plans 9+; 0 for other plans).
inline unsigned gold_for_next_emblem(cambc::champion& me) {
    if constexpr (SHOP_PLAN < 9) return 0;
    const auto plan = shop_plan_for(me);
    const auto items = me.item_count();
    for (int i = 0; i < plan.n_steps; ++i) {
        const auto& step = plan.steps[i];
        if (items[step.emblem] != 0) continue;
        return (5 - std::min(4u, static_cast<unsigned>(items[step.shard]))) * step.shard.cost();
    }
    return (5 - std::min(4u, static_cast<unsigned>(items[plan.main]))) * plan.main.cost();
}

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
    if constexpr (SHOP_PLAN >= 9) return shop_emblem_plan(me);
    // Test builds may change the plan (SHOP_PLAN): first complete one emblem of `first` (its focus
    // variant), then spend the rest on `main`. Plan 0: Bruiser health shards only.
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
