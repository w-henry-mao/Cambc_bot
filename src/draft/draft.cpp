#include "draft.h"
#include <algorithm>
#include <string_view>
#include <vector>
#include "../core/rng.h"
void play_draft() {
    const auto chosen = cambc::draft::current_draft(cambc::team::ally);
    std::vector<cambc::champion_draft> available;
    for (int i = 0; i < 11; ++i) {
        auto pick = static_cast<cambc::champion_draft>(i);
        if (std::find(chosen.begin(), chosen.end(), pick) == chosen.end()) available.push_back(pick);
    }
    cambc::draft_memory = std::string_view("", 0);
    cambc::draft::pick(available.at(random_number(available.size())));
}
