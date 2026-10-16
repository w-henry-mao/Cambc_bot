#include "draft.h"
#include <algorithm>
#include <string_view>
#include <vector>
#include "../constants/draft_constants.h"
#include "../constants/test_hooks.h"
namespace {
bool already_picked(const std::vector<cambc::champion_draft>& chosen, cambc::champion_draft pick) {
    return std::find(chosen.begin(), chosen.end(), pick) != chosen.end();
}
}
void play_draft() {
    const auto chosen = cambc::draft::current_draft(cambc::team::ally);
    cambc::draft_memory = std::string_view("", 0);
    // Duel test builds may take one champion first (constants/test_hooks.h); -1 in normal builds.
    if constexpr (DUEL_FIRST_PICK >= 0) {
        const auto first = static_cast<cambc::champion_draft>(DUEL_FIRST_PICK);
        if (!already_picked(chosen, first)) { cambc::draft::pick(first); return; }
    }
    for (const auto pick : constants::draft_priority) {
        if (!already_picked(chosen, pick)) {
            cambc::draft::pick(pick);
            return;
        }
    }
    // Priority list exhausted: take the lowest-numbered champion still free.
    for (int i = 0; i < 11; ++i) {
        const auto pick = static_cast<cambc::champion_draft>(i);
        if (!already_picked(chosen, pick)) {
            cambc::draft::pick(pick);
            return;
        }
    }
}
