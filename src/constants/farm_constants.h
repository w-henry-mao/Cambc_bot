#pragma once
#include <cambc.h>
#include <cstdint>
#include "game_rules.h"
#include "tunable_constants.h"

namespace farm_constants {
    constexpr float kGoldWeight = tunable_constants::kGoldWeight;
    constexpr float kRoundDiscount = tunable_constants::kRoundDiscount;
    constexpr float kRespawnWalkFactor = tunable_constants::kRespawnWalkFactor;
    constexpr int kCampSearchRange = tunable_constants::kCampSearchRange;
    constexpr int kCaptureTimeNormal = game_rules::kCaptureRounds;
    constexpr int kCaptureTimeWithEmblem = game_rules::kCaptureRoundsWithEmblem;
    constexpr std::uint32_t kBlockedSkipRounds = tunable_constants::kBlockedSkipRounds;
    
    // Tunable target preference (lower is selected first).
    constexpr int get_camp_priority_rank(cambc::camp_type type) {
        switch (type) {
            case cambc::camp_type::elk: return tunable_constants::kElkCampPriority;
            case cambc::camp_type::golem: return tunable_constants::kGolemCampPriority;
            case cambc::camp_type::wolf: return tunable_constants::kWolfCampPriority;
            case cambc::camp_type::monolith: return tunable_constants::kMonolithCampPriority;
            case cambc::camp_type::gold_cluster:
            case cambc::camp_type::gem_cluster: return tunable_constants::kResourceClusterCampPriority;
            default: return tunable_constants::kResourceClusterCampPriority;
        }
    }
    
    constexpr bool kRangedWalkToPriority = tunable_constants::kRangedWalkToPriority;
}
