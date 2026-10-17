#include "altar_locks.h"
#include <cambc.h>
#include "../constants/brain_constants.h"
#include "../core/roster.h"

void place_altar_lock() {
    if constexpr (constants::kAltarLockPlan == 0) return;
    else {
        try {
            auto& me = cambc::self();
            if (!me.is_alive()) return;
            const auto& entry = me.consumables()[cambc::consumable_type::altar_lock];
            if (entry.count() == 0) return;
            const auto here = me.position();
            for (const auto altar : cambc::map::altars().positions()) {
                if (altar.chebyshev_distance(here) > constants::kAltarLockRange || cambc::vision::has_altar_lock(altar)) continue;
                bool place = (constants::kAltarLockPlan & 1) != 0;
                if (!place && (constants::kAltarLockPlan & 2) != 0)
                    for (const auto& e : roster_enemies())
                        if (in_view(e) && e.is_submitting_gems()
                            && static_cast<int>(e.position().chebyshev_distance(altar)) <= static_cast<int>(cambc::constants::submit_gems_range)) {
                            place = true;
                            break;
                        }
                if (place) { entry.use(altar); return; }
            }
        } catch (const cambc::error&) {}
    }
}
