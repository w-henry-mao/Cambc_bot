#pragma once
#include <array>
#include <cambc.h>
namespace constants {
// Picks in order of preference; each draft round takes the first one we have not picked yet.
// The top two teams' core (Ren, Stellar, Cassi & Issac, Ryth) plus Tavi: 177/204 vs 157/204 for
// the old Varg, Cassi & Issac, Willow, Stellar, Ryth (README, Draft).
inline constexpr std::array draft_priority{
    cambc::champion_draft::ren,
    cambc::champion_draft::stellar,
    cambc::champion_draft::cassi_and_issac,
    cambc::champion_draft::ryth,
    cambc::champion_draft::tavi,
};
}
