#pragma once
// Dummy Ryth that first buys a Durability emblem (5 health shards) on spawn, then stands in contact.
#define PROBE_HAS_READY
constexpr int kPick = 0, kFighterType = 0, kStopDistance = 6;
inline bool bought = false;
inline bool probe_ready(Probe& p) {
    if (bought) return true;
    const auto shard = cambc::item_type::durability_shard_of_health;
    if (p.me.gold_count() >= 5 * shard.cost() && p.me.can_buy(shard, 5)) {
        p.attempt("buy_durability", [&] { p.me.buy(shard, 5); return true; });
        bought = true;
        std::cout << "ITEMS emblem=" << p.me.item_count()[cambc::item_type::durability_emblem_of_health] << "\n";
    }
    return bought;
}
inline void probe_act(Probe&) {}
