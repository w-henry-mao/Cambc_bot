#pragma once
#include <cambc.h>
#include <algorithm>
#include <optional>
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
//   15 Jungle, Durability (chosen stat), then Bruiser shards for selected champions or Power shards
// The emblem plan for `me`: the emblems to finish in order, then the shard bought with the rest.
struct EmblemPlan {
    struct Step { cambc::item_type shard, emblem; };
    Step steps[4];
    int n_steps = 0;
    cambc::item_type main;
};
inline EmblemPlan emblem_plan(const cambc::champion& me) {
    using it = cambc::item_type;
    using ct = cambc::champion_type;
    using Step = EmblemPlan::Step;
    const auto type = me.champion_name();
    const bool focus = type == ct::cassi || type == ct::stellar || type == ct::prism || type == ct::ren
        || ((SHOP_PLAN == 13 || SHOP_PLAN == 14) && type == ct::willow)
        || (TAVI_FOCUS && type == ct::tavi);   // test: Tavi's damage is its spell
    const Step power = focus ? Step{it::power_shard_of_focus, it::power_emblem_of_focus}
                             : Step{it::power_shard_of_strength, it::power_emblem_of_strength};
    const Step jungle = focus ? Step{it::jungle_shard_of_focus, it::jungle_emblem_of_focus}
                              : Step{it::jungle_shard_of_strength, it::jungle_emblem_of_strength};
    const Step dur_main = focus ? Step{it::durability_shard_of_focus, it::durability_emblem_of_focus}
                                : Step{it::durability_shard_of_strength, it::durability_emblem_of_strength};
    const Step dur_hp{it::durability_shard_of_health, it::durability_emblem_of_health};
    const Step bru_hp{it::bruiser_shard_of_health, it::bruiser_emblem_of_health};
    EmblemPlan plan;
    plan.main = power.shard;
    if constexpr (SHOP_PLAN == 15) {
        const bool chosen_focus = type == ct::cassi || type == ct::prism || type == ct::stellar
            || type == ct::tavi || type == ct::willow;
        const bool bruiser = type == ct::cassi || type == ct::ryth || type == ct::prism
            || type == ct::stellar || type == ct::tavi || type == ct::willow || type == ct::ren;
        const auto jungle_shard = chosen_focus ? it::jungle_shard_of_focus : it::jungle_shard_of_strength;
        const auto jungle_emblem = chosen_focus ? it::jungle_emblem_of_focus : it::jungle_emblem_of_strength;
        const auto durability_shard = chosen_focus ? it::durability_shard_of_focus : it::durability_shard_of_strength;
        const auto durability_emblem = chosen_focus ? it::durability_emblem_of_focus : it::durability_emblem_of_strength;
        plan.steps[0] = Step{jungle_shard, jungle_emblem};
        plan.steps[1] = Step{durability_shard, durability_emblem};
        plan.n_steps = 2;
        plan.main = bruiser
            ? (chosen_focus ? it::bruiser_shard_of_focus : it::bruiser_shard_of_strength)
            : (chosen_focus ? it::power_shard_of_focus : it::power_shard_of_strength);
        return plan;
    }
    switch (SHOP_PLAN) {
        case 10: plan.steps[0] = jungle; plan.steps[1] = power; plan.steps[2] = dur_main; plan.steps[3] = bru_hp; plan.n_steps = 4; break;
        case 11: plan.steps[0] = power; plan.steps[1] = dur_hp; plan.n_steps = 2; break;
        case 12: plan.steps[0] = dur_hp; plan.n_steps = 1; plan.main = bru_hp.shard; break;
        case 14: plan.steps[0] = jungle; plan.steps[1] = power; plan.steps[2] = dur_hp; plan.steps[3] = bru_hp; plan.n_steps = 4; break;
        default: plan.steps[0] = power; plan.steps[1] = dur_hp; plan.steps[2] = bru_hp; plan.n_steps = 3; break;
    }
    return plan;
}
// Gold that finishes our next planned emblem (its missing shards); nullopt once every planned
// emblem is done, or for the shard-only test plans.
inline std::optional<unsigned> next_emblem_gold(const cambc::champion& me) {
    if constexpr (SHOP_PLAN < 9) return std::nullopt;
    const auto plan = emblem_plan(me);
    const auto items = me.item_count();
    for (int i = 0; i < plan.n_steps; ++i) {
        if (items[plan.steps[i].emblem] != 0) continue;
        const unsigned need = 5 - std::min(4u, static_cast<unsigned>(items[plan.steps[i].shard]));
        return need * plan.steps[i].shard.cost();
    }
    return std::nullopt;
}
// SHOP_RECALL_GOLD 1 (states/recall.cpp): gold for the next emblem, or five main shards once
// every planned emblem is done; 0 for the shard-only test plans.
inline unsigned gold_for_next_emblem(const cambc::champion& me) {
    if constexpr (SHOP_PLAN < 9) return 0;
    return next_emblem_gold(me).value_or(5 * emblem_plan(me).main.cost());
}
inline bool shop_emblem_plan(cambc::champion& me) {
    const auto plan = emblem_plan(me);
    const auto& steps = plan.steps;
    const int n_steps = plan.n_steps;
    const auto main = plan.main;
    bool bought = false;
    const auto items = me.item_count();
    for (int i = 0; i < n_steps; ++i) {
        if (items[steps[i].emblem] != 0) continue;
        const unsigned need = 5 - std::min(4u, static_cast<unsigned>(items[steps[i].shard]));
        const unsigned n = std::min(need, me.gold_count() / steps[i].shard.cost());
        if (n && me.can_buy(steps[i].shard, n)) { me.buy(steps[i].shard, n); bought = true; }
        if (n < need) return bought;   // finish this emblem before the next step
    }
    const unsigned count = me.gold_count() / main.cost();
    if (!count || !me.can_buy(main, count)) return bought;
    me.buy(main, count);
    return true;
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
