#include "draft.h"
#include "../core/rng.h"
#include <algorithm>
#include <optional>
#include <span>
#include <vector>

namespace {
enum class AltarMode { Single, Zone, Normal };
enum class ResourceMode { GemsOnly, CampsOnly, Normal };

struct Classification {
    AltarMode altar_mode = AltarMode::Normal;
    ResourceMode resource_mode = ResourceMode::Normal;
    bool combative = true;
    std::optional<cambc::vec2> altar_position;
};

Classification classify_map() {
    Classification result;
    const auto altar_positions = cambc::map::altars().positions();
    if (altar_positions.size() == 1) {
        result.altar_mode = AltarMode::Single;
        result.altar_position = altar_positions.front();
    } else if (altar_positions.size() >= 2) {
        std::int32_t min_x = altar_positions.front().x;
        std::int32_t max_x = min_x;
        std::int32_t min_y = altar_positions.front().y;
        std::int32_t max_y = min_y;
        for (const auto position : altar_positions) {
            min_x = std::min(min_x, position.x);
            max_x = std::max(max_x, position.x);
            min_y = std::min(min_y, position.y);
            max_y = std::max(max_y, position.y);
        }

        const auto width = max_x - min_x + 1;
        const auto height = max_y - min_y + 1;
        if (width <= 10 && height <= 10) {
            result.altar_mode = AltarMode::Zone;
            std::int32_t center_x;
            std::int32_t center_y;
            if (width % 2 == 1 && height % 2 == 1) {
                center_x = min_x + width / 2;
                center_y = min_y + height / 2;
            } else if (width % 2 == 1) {
                center_x = min_x + width / 2;
                const auto bottom = min_y + height / 2 - 1;
                const auto top = min_y + height / 2;
                center_y = random_number(2) == 0 ? bottom : top;
            } else if (height % 2 == 1) {
                const auto left = min_x + width / 2 - 1;
                const auto right = min_x + width / 2;
                center_x = random_number(2) == 0 ? left : right;
                center_y = min_y + height / 2;
            } else {
                const auto left = min_x + width / 2 - 1;
                const auto right = min_x + width / 2;
                const auto bottom = min_y + height / 2 - 1;
                const auto top = min_y + height / 2;
                center_x = random_number(2) == 0 ? left : right;
                center_y = random_number(2) == 0 ? bottom : top;
            }
            result.altar_position = cambc::vec2{center_x, center_y};
        }
    }

    const auto groups = cambc::map::camp_groups();
    bool has_camps = false;
    bool only_gems = true;
    bool only_camps = true;
    for (const auto& group : groups) {
        for (const auto& camp : group.camps()) {
            has_camps = true;
            const auto kind = camp.kind();
            only_gems &= kind == cambc::camp_type::gem_cluster;
            only_camps &= kind == cambc::camp_type::wolf || kind == cambc::camp_type::elk ||
                          kind == cambc::camp_type::golem || kind == cambc::camp_type::monolith;
        }
    }
    if (has_camps && only_gems) result.resource_mode = ResourceMode::GemsOnly;
    else if (has_camps && only_camps) result.resource_mode = ResourceMode::CampsOnly;
    return result;
}

void write_draft_memory(const Classification& classification) {
    std::byte buffer[3];
    buffer[0] = static_cast<std::byte>(static_cast<int>(classification.altar_mode) |
        (static_cast<int>(classification.resource_mode) << 2) |
        (classification.combative ? (1 << 4) : 0));
    if (classification.altar_position) {
        buffer[1] = static_cast<std::byte>(classification.altar_position->x + 1);
        buffer[2] = static_cast<std::byte>(classification.altar_position->y + 1);
    } else {
        buffer[1] = std::byte{0};
        buffer[2] = std::byte{0};
    }
    cambc::draft_memory_buffer() = std::span<const std::byte>{buffer, 3};
}

void pick_champion() {
    const auto team_size = cambc::draft::team_size();
    const auto current_draft = cambc::draft::current_draft(cambc::team::ally);
    const auto round = current_draft.size();
    if (round >= team_size) return;

    const std::vector<cambc::champion_draft> pool = {
        cambc::champion_draft::heart_of_ruin,
        cambc::champion_draft::vendetta,
        cambc::champion_draft::marcella,
        cambc::champion_draft::vesper_vale
    };
    std::vector<cambc::champion_draft> available;
    for (std::size_t i = 0; i < static_cast<std::size_t>(team_size - 1) && i < pool.size(); ++i) {
        available.push_back(pool[i]);
    }
    available.erase(std::remove_if(available.begin(), available.end(), [&](auto champion) {
        return std::find(current_draft.begin(), current_draft.end(), champion) != current_draft.end();
    }), available.end());

    const auto pick = round == team_size - 1
        ? cambc::champion_draft::prism
        : available[random_number(available.size())];
    try {
        cambc::draft::pick(pick);
    } catch (const cambc::error&) {
        // A rejected pick is retried on the next draft turn.
    }
}
}

void play_draft() {
    if (cambc::draft::current_draft(cambc::team::ally).empty()) {
        write_draft_memory(classify_map());
    }
    pick_champion();
}
