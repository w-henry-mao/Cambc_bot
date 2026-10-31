#include "shop.h"
#include <cambc.h>
#include <algorithm>
#include <array>
#include <cstdint>

namespace {
struct PlannedEmblem {
    cambc::item_family family;
    cambc::item_stat stat;
};

constexpr std::array prism_plan{
    PlannedEmblem{cambc::item_family::capture, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::prospector, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::durability, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::collectors, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::skirmish, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::hunter, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::treasure, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::home, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::bruiser, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::power, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::monster, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::jungle, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::tribute, cambc::item_stat::health},
};

constexpr std::array vendetta_plan{
    PlannedEmblem{cambc::item_family::durability, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::bruiser, cambc::item_stat::strength},
    PlannedEmblem{cambc::item_family::power, cambc::item_stat::strength},
    PlannedEmblem{cambc::item_family::collectors, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::skirmish, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::jungle, cambc::item_stat::strength},
    PlannedEmblem{cambc::item_family::monster, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::home, cambc::item_stat::strength},
    PlannedEmblem{cambc::item_family::hunter, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::treasure, cambc::item_stat::strength},
    PlannedEmblem{cambc::item_family::tribute, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::prospector, cambc::item_stat::strength},
    PlannedEmblem{cambc::item_family::capture, cambc::item_stat::strength},
};

constexpr std::array marcella_plan{
    PlannedEmblem{cambc::item_family::durability, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::bruiser, cambc::item_stat::focus},
    PlannedEmblem{cambc::item_family::power, cambc::item_stat::focus},
    PlannedEmblem{cambc::item_family::collectors, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::skirmish, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::jungle, cambc::item_stat::focus},
    PlannedEmblem{cambc::item_family::monster, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::home, cambc::item_stat::focus},
    PlannedEmblem{cambc::item_family::hunter, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::treasure, cambc::item_stat::focus},
    PlannedEmblem{cambc::item_family::tribute, cambc::item_stat::health},
    PlannedEmblem{cambc::item_family::prospector, cambc::item_stat::focus},
    PlannedEmblem{cambc::item_family::capture, cambc::item_stat::focus},
};

template <typename Champion>
void buy_alternating_bruiser(Champion& me, cambc::enum_map<cambc::item_type>& inventory,
                             std::uint32_t gold, cambc::item_stat second_stat) {
    const auto health_shard = cambc::get_item_enum(cambc::item_family::bruiser, cambc::item_stat::health);
    const auto health_emblem = cambc::get_item_enum(cambc::item_family::bruiser, cambc::item_stat::health, false);
    const auto second_shard = cambc::get_item_enum(cambc::item_family::bruiser, second_stat);
    const auto second_emblem = cambc::get_item_enum(cambc::item_family::bruiser, second_stat, false);
    while (gold >= 30) {
        auto shard = health_shard;
        auto emblem = health_emblem;
        const auto alternating_emblems = inventory[health_emblem] + inventory[second_emblem] - 1;
        if (inventory[health_shard] == 0 && inventory[second_shard] == 0 &&
            alternating_emblems % 2 != 0) {
            shard = second_shard;
            emblem = second_emblem;
        } else if (inventory[health_shard] == 0 && inventory[second_shard] > 0) {
            shard = second_shard;
            emblem = second_emblem;
        }

        const auto count = std::min<std::uint32_t>(5 - inventory[shard], gold / 30);
        me.buy(shard, count);
        gold -= count * 30;
        inventory[shard] += count;
        if (inventory[shard] < 5) return;
        inventory[emblem] += 1;
        inventory[shard] = 0;
    }
}

template <typename Champion, std::size_t N>
void buy_plan(Champion& me, const std::array<PlannedEmblem, N>& plan,
              cambc::enum_map<cambc::item_type>& inventory, std::uint32_t& gold,
              bool prism, bool tribute_due, cambc::item_stat alternate_stat) {
    const auto tribute_shard = cambc::get_item_enum(cambc::item_family::tribute, cambc::item_stat::health);
    const auto tribute_emblem = cambc::get_item_enum(cambc::item_family::tribute, cambc::item_stat::health, false);
    const bool tribute_owned = inventory[tribute_shard] > 0 || inventory[tribute_emblem] > 0;
    if (prism && inventory[tribute_shard] > 0 && inventory[tribute_emblem] == 0) {
        const auto count = std::min<std::uint32_t>(5 - inventory[tribute_shard], gold / 30);
        if (count) me.buy(tribute_shard, count);
        return;
    }
    for (const auto& target : plan) {
        const auto shard = cambc::get_item_enum(target.family, target.stat);
        const auto emblem = cambc::get_item_enum(target.family, target.stat, false);
        if (inventory[emblem] > 0) continue;

        const auto shards = inventory[shard];
        if (prism && tribute_due && !tribute_owned && shards == 0) {
            const auto count = std::min<std::uint32_t>(5, gold / 30);
            if (count) me.buy(tribute_shard, count);
            return;
        }

        const auto needed = 5 - shards;
        const auto count = std::min<std::uint32_t>(needed, gold / 30);
        if (count) {
            me.buy(shard, count);
            gold -= count * 30;
            inventory[shard] += count;
        }
        if (count < needed) return;
        inventory[emblem] += 1;
        inventory[shard] = 0;

        if (prism && tribute_due && !tribute_owned) {
            const auto tribute_count = std::min<std::uint32_t>(5, gold / 30);
            if (tribute_count) me.buy(tribute_shard, tribute_count);
            return;
        }
    }

    if (prism && !tribute_owned && tribute_due) {
        const auto count = std::min<std::uint32_t>(5, gold / 30);
        if (count) me.buy(tribute_shard, count);
        return;
    }
    if (prism && inventory[cambc::get_item_enum(cambc::item_family::jungle,
                                                 cambc::item_stat::health, false)] > 0) {
        const auto shard = cambc::get_item_enum(cambc::item_family::bruiser, cambc::item_stat::health);
        const auto count = gold / 30;
        if (count) me.buy(shard, count);
    } else if (!prism) {
        buy_alternating_bruiser(me, inventory, gold, alternate_stat);
    }
}
}

void shop_at_spawn_or_death() {
    auto& me = cambc::self();
    if (me.is_alive() && !cambc::map::spawn_region(cambc::team::ally).is_set(me.position())) return;

    const auto champion = me.champion_name();
    auto inventory = me.item_count();
    auto gold = me.gold_count();
    if (gold < 30) return;

    if (champion == cambc::champion_type::prism) {
        const bool tribute_due = me.gem_count() + cambc::global_submitted_gems(cambc::team::ally) > 600;
        buy_plan(me, prism_plan, inventory, gold, true, tribute_due, cambc::item_stat::health);
    } else if (champion == cambc::champion_type::vendetta || champion == cambc::champion_type::vesper_vale) {
        buy_plan(me, vendetta_plan, inventory, gold, false, false, cambc::item_stat::strength);
    } else if (champion == cambc::champion_type::marcella || champion == cambc::champion_type::heart_of_ruin) {
        buy_plan(me, marcella_plan, inventory, gold, false, false, cambc::item_stat::focus);
    }
}
