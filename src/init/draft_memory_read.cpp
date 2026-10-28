#include "stages.h"
#include "../core/bot_memory.h"
#include "../debug/log.h"
#include <iostream>

bool stage_draft_memory_read(MatchData& match_data, BotMemory& memory) {
    // Skip if already initialized
    if (memory.init_stage > 0) {
        return true;
    }

    // Load draft memory
    cambc::draft_memory_buffer buffer;

    // Check if empty
    if (buffer.is_empty()) {
        if constexpr (debug::kEnableLogging) {
            std::cout << "Draft memory buffer is empty, skipping read" << std::endl;
        }
        return true;
    }

    // Read and decode byte 0
    const std::byte byte0 = buffer[0];
    const int altar_mode = static_cast<int>(byte0) & 0x3;
    const int resource_mode = (static_cast<int>(byte0) >> 2) & 0x3;
    const bool combative = (static_cast<int>(byte0) & (1 << 4)) != 0;

    // Read and decode bytes 1-2 (altar position)
    const std::byte byte1 = buffer[1];
    const std::byte byte2 = buffer[2];
    int altar_x = -1;
    int altar_y = -1;

    if (static_cast<int>(byte1) != 0 && static_cast<int>(byte2) != 0) {
        altar_x = static_cast<int>(byte1) - 1;
        altar_y = static_cast<int>(byte2) - 1;
    }

    // Store in BotMemory
    memory.draft_memory.altar_mode = altar_mode;
    memory.draft_memory.resource_mode = resource_mode;
    memory.draft_memory.combative = combative;
    memory.draft_memory.altar_position_x = altar_x;
    memory.draft_memory.altar_position_y = altar_y;

    // Store champion name
    try {
        memory.champion_name = cambc::to_string(cambc::self().champion_name());
    } catch (...) {
        memory.champion_name = "UNKNOWN";
    }

    // Mark as initialized
    memory.init_stage = 1;

    // Log decoded values
    if constexpr (debug::kEnableLogging && debug::kLogInit) {
        debug::log_with_name(memory.champion_name, "Draft memory read: [" + std::to_string(static_cast<int>(byte0)) + ", " +
                          std::to_string(static_cast<int>(byte1)) + ", " + std::to_string(static_cast<int>(byte2)) + "]");
        debug::log_with_name(memory.champion_name, "  Decoded altar_mode=" + std::to_string(altar_mode) +
                          " (0=Single, 1=Zone, 2=Normal)");
        debug::log_with_name(memory.champion_name, "  Decoded resource_mode=" + std::to_string(resource_mode) +
                          " (0=GemsOnly, 1=CampsOnly, 2=Normal)");
        debug::log_with_name(memory.champion_name, "  Decoded combative=" + std::to_string(combative));
        if (altar_x >= 0 && altar_y >= 0) {
            debug::log_with_name(memory.champion_name, "  Decoded altar position: (" + std::to_string(altar_x) + ", " +
                              std::to_string(altar_y) + ")");
        } else {
            debug::log_with_name(memory.champion_name, "  No altar position stored");
        }
    }

    return true;
}
