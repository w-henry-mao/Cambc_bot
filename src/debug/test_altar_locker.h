#pragma once
// Test opponent only (TEST_ALTAR_LOCKER): place a carried altar lock on any unlocked altar within
// two tiles. Lets matches check that our tribute state breaks enemy locks.
#include <cambc.h>
#include <iostream>
#include "../constants/test_hooks.h"

inline void test_place_altar_lock() {
    if constexpr (TEST_ALTAR_LOCKER) {
        try {
            auto& me = cambc::self();
            if (!me.is_alive()) return;
            const auto& entry = me.consumables()[cambc::consumable_type::altar_lock];
            if (entry.count() == 0) return;
            for (const auto altar : cambc::map::altars().positions())
                if (altar.chebyshev_distance(me.position()) <= 2 && !cambc::vision::has_altar_lock(altar)) {
                    if (entry.use(altar) && SCORE_TRACE) std::cout << "LOCK_PLACED r=" << cambc::round_number() << '\n';
                    return;
                }
        } catch (const cambc::error&) {}
    }
}
