#pragma once
#include <array>
#include <cambc.h>
namespace constants {
// Picks in order of preference; each draft round takes the first one we have not picked yet.
inline constexpr std::array draft_priority{
    cambc::champion_draft::varg,
    cambc::champion_draft::cassi_and_issac,
    cambc::champion_draft::willow,
    cambc::champion_draft::stellar,
    cambc::champion_draft::ryth,
};
}
