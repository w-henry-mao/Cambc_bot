#include "draft.h"
#include "../core/rng.h"
#include "../debug/log.h"
#include "../debug/overlay.h"
#include <vector>
#include <algorithm>
#include <span>
#include <iostream>
#include <optional>
#include <string>

namespace {
    enum class AltarMode { Single, Zone, Normal };
    enum class ResourceMode { GemsOnly, CampsOnly, Normal };

    struct Classification {
        AltarMode altar_mode;
        ResourceMode resource_mode;
        bool combative;
        std::optional<cambc::vec2> altar_position;
        std::string center_label;
    };

    Classification classify_map() {
        Classification result;
        result.combative = true;
        result.center_label = "";

        // Altar mode classification
        const auto& altars = cambc::map::altars();
        const auto altar_positions = altars.positions();
        const auto altar_count = altar_positions.size();

        if (altar_count == 1) {
            result.altar_mode = AltarMode::Single;
            result.altar_position = altar_positions[0];
            result.center_label = "";
        } else if (altar_count >= 2) {
            // Calculate bounding box
            std::int32_t min_x = altar_positions[0].x;
            std::int32_t max_x = altar_positions[0].x;
            std::int32_t min_y = altar_positions[0].y;
            std::int32_t max_y = altar_positions[0].y;
            for (const auto& pos : altar_positions) {
                min_x = std::min(min_x, pos.x);
                max_x = std::max(max_x, pos.x);
                min_y = std::min(min_y, pos.y);
                max_y = std::max(max_y, pos.y);
            }
            const std::int32_t width = max_x - min_x + 1;
            const std::int32_t height = max_y - min_y + 1;

            if (width <= 10 && height <= 10) {
                result.altar_mode = AltarMode::Zone;
                // Calculate center, handling even dimensions
                std::int32_t center_x, center_y;
                std::string center_label;

                if (width % 2 == 1 && height % 2 == 1) {
                    // Both odd: exact center
                    center_x = min_x + width / 2;
                    center_y = min_y + height / 2;
                    center_label = "exact center";
                } else if (width % 2 == 1) {
                    // Width odd, height even
                    center_x = min_x + width / 2;
                    const std::int32_t bottom = min_y + height / 2 - 1;
                    const std::int32_t top = min_y + height / 2;
                    center_y = random_number(2) == 0 ? bottom : top;
                    center_label = center_y == bottom ? "bottom" : "top";
                } else if (height % 2 == 1) {
                    // Width even, height odd
                    const std::int32_t left = min_x + width / 2 - 1;
                    const std::int32_t right = min_x + width / 2;
                    center_x = random_number(2) == 0 ? left : right;
                    center_y = min_y + height / 2;
                    center_label = center_x == left ? "left" : "right";
                } else {
                    // Both even: four candidates
                    const std::int32_t left = min_x + width / 2 - 1;
                    const std::int32_t right = min_x + width / 2;
                    const std::int32_t bottom = min_y + height / 2 - 1;
                    const std::int32_t top = min_y + height / 2;
                    center_x = random_number(2) == 0 ? left : right;
                    center_y = random_number(2) == 0 ? bottom : top;
                    if (center_x == left && center_y == bottom) {
                        center_label = "bottom left";
                    } else if (center_x == right && center_y == bottom) {
                        center_label = "bottom right";
                    } else if (center_x == left && center_y == top) {
                        center_label = "top left";
                    } else {
                        center_label = "top right";
                    }
                }
                result.altar_position = cambc::vec2{center_x, center_y};
                result.center_label = center_label;
            } else {
                result.altar_mode = AltarMode::Normal;
                result.center_label = "";
            }
        } else {
            result.altar_mode = AltarMode::Normal;
            result.center_label = "";
        }

        // Resource mode classification
        const auto camp_groups = cambc::map::camp_groups();
        bool has_camps = false;
        bool has_only_gems = true;
        bool has_only_camps = true;

        for (const auto& group : camp_groups) {
            for (const auto& camp : group.camps()) {
                has_camps = true;
                const auto kind = camp.kind();
                if (kind != cambc::camp_type::gem_cluster) {
                    has_only_gems = false;
                }
                if (kind != cambc::camp_type::wolf &&
                    kind != cambc::camp_type::elk &&
                    kind != cambc::camp_type::golem &&
                    kind != cambc::camp_type::monolith) {
                    has_only_camps = false;
                }
            }
        }

        if (!has_camps) {
            result.resource_mode = ResourceMode::Normal;
        } else if (has_only_gems) {
            result.resource_mode = ResourceMode::GemsOnly;
        } else if (has_only_camps) {
            result.resource_mode = ResourceMode::CampsOnly;
        } else {
            result.resource_mode = ResourceMode::Normal;
        }

        return result;
    }

    void write_draft_memory(const Classification& classification) {
        std::byte buffer[3];

        // Byte 0: altar mode (bits 0-1), resource mode (bits 2-3), combative (bit 4)
        buffer[0] = static_cast<std::byte>(
            (static_cast<int>(classification.altar_mode) & 0x3) |
            ((static_cast<int>(classification.resource_mode) & 0x3) << 2) |
            (classification.combative ? (1 << 4) : 0)
        );

        // Bytes 1-2: altar position with +1 offset (0 means none)
        if (classification.altar_position) {
            buffer[1] = static_cast<std::byte>(classification.altar_position->x + 1);
            buffer[2] = static_cast<std::byte>(classification.altar_position->y + 1);
        } else {
            buffer[1] = std::byte{0};
            buffer[2] = std::byte{0};
        }

        cambc::draft_memory_buffer() = std::span<const std::byte>{buffer, 3};

        if constexpr (debug::kEnableLogging && debug::kLogDraft) {
            std::cout << "[DRAFT] Memory written: [" << static_cast<int>(buffer[0]) << ", "
                      << static_cast<int>(buffer[1]) << ", " << static_cast<int>(buffer[2]) << "]" << std::endl;
            std::cout << "[DRAFT]   Byte 0: altar_mode=" << static_cast<int>(classification.altar_mode)
                      << ", resource_mode=" << static_cast<int>(classification.resource_mode)
                      << ", combative=" << classification.combative << std::endl;
            if (classification.altar_position) {
                std::cout << "[DRAFT]   Bytes 1-2: altar position ("
                          << classification.altar_position->x << ", " << classification.altar_position->y
                          << ") stored as (" << static_cast<int>(buffer[1]) << ", " << static_cast<int>(buffer[2]) << ")" << std::endl;
            } else {
                std::cout << "[DRAFT]   Bytes 1-2: no altar position stored" << std::endl;
            }
        }
    }

    void log_classification(const Classification& classification) {
        if constexpr (!debug::kEnableLogging) return;

        // Log altar mode
        switch (classification.altar_mode) {
            case AltarMode::Single:
                debug::log_draft("Altar mode: Single");
                if (classification.altar_position) {
                    std::cout << "  Position: (" << classification.altar_position->x << ", " << classification.altar_position->y << ")" << std::endl;
                }
                break;
            case AltarMode::Zone:
                debug::log_draft("Altar mode: Zone");
                if (classification.altar_position) {
                    std::cout << "  Center: (" << classification.altar_position->x << ", " << classification.altar_position->y << ") - " << classification.center_label << std::endl;
                }
                break;
            case AltarMode::Normal:
                debug::log_draft("Altar mode: Normal");
                break;
        }

        // Log resource mode
        switch (classification.resource_mode) {
            case ResourceMode::GemsOnly:
                debug::log_draft("Resource mode: Gems only");
                break;
            case ResourceMode::CampsOnly:
                debug::log_draft("Resource mode: Camps only");
                break;
            case ResourceMode::Normal:
                debug::log_draft("Resource mode: Normal");
                break;
        }

        // Draw debug dot
        if (classification.altar_position) {
            debug::draw_dot(*classification.altar_position);
        }
    }

    void pick_champion() {
        const auto team_size = cambc::draft::team_size();
        const auto current_draft = cambc::draft::current_draft(cambc::team::ally);
        const auto round = current_draft.size();

        if (round >= team_size) {
            return;
        }

        // Full champion pool in fixed order
        std::vector<cambc::champion_draft> full_pool = {
            cambc::champion_draft::heart_of_ruin,
            cambc::champion_draft::vendetta,
            cambc::champion_draft::marcella,
            cambc::champion_draft::vesper_vale
        };

        // Only the first (team_size - 1) champions are available for random selection
        std::vector<cambc::champion_draft> available_pool;
        for (std::size_t i = 0; i < static_cast<std::size_t>(team_size - 1) && i < full_pool.size(); ++i) {
            available_pool.push_back(full_pool[i]);
        }

        // Remove already-picked champions from available pool
        available_pool.erase(
            std::remove_if(available_pool.begin(), available_pool.end(),
                [&current_draft](cambc::champion_draft champ) {
                    return std::find(current_draft.begin(), current_draft.end(), champ) != current_draft.end();
                }),
            available_pool.end()
        );

        cambc::champion_draft pick;
        if (round == team_size - 1) {
            // Last pick: always Prism
            pick = cambc::champion_draft::prism;
        } else {
            // Random pick from remaining available pool
            const auto idx = random_number(available_pool.size());
            pick = available_pool[idx];
        }

        try {
            cambc::draft::pick(pick);
        } catch (const cambc::error& e) {
            // Handle error gracefully (e.g., draft_already_picked)
            if constexpr (debug::kEnableLogging) {
                std::cout << "Draft pick error: " << e.what() << std::endl;
            }
        }
    }
}

void play_draft() {
    const auto current_draft = cambc::draft::current_draft(cambc::team::ally);
    const auto round = current_draft.size();

    // Round 0: classify map and write memory
    if (round == 0) {
        const auto classification = classify_map();
        log_classification(classification);
        write_draft_memory(classification);
    }

    // Pick champion for this round
    pick_champion();
}
