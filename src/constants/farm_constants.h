#pragma once
#include <cambc.h>
#include <cstdint>

namespace farm_constants {
    constexpr float kGoldWeight = 0.2f;
    constexpr float kRoundDiscount = 0.95f;
    constexpr float kRespawnWalkFactor = 0.8f;
    constexpr int kCampSearchRange = 10000;  // Effectively infinite
    constexpr int kCaptureTimeNormal = 5;
    constexpr int kCaptureTimeWithEmblem = 3;
    constexpr std::uint32_t kBlockedSkipRounds = 30;
    
    // Camp priority rank (lower is better). 
    // This is strength ÷ max health and must be updated if the game is patched.
    constexpr int get_camp_priority_rank(cambc::camp_type type) {
        switch (type) {
            case cambc::camp_type::elk: return 1;      // 0.0250
            case cambc::camp_type::golem: return 2;    // 0.0240
            case cambc::camp_type::wolf: return 3;     // 0.0148
            case cambc::camp_type::monolith: return 4; // 0.0100
            case cambc::camp_type::gold_cluster:
            case cambc::camp_type::gem_cluster: return 5; // 0
            default: return 5;
        }
    }
    
    constexpr bool kRangedWalkToPriority = false;
}
