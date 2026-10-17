#include "team_comms.h"
#include <array>
#include <cambc.h>
#include <cstddef>
#include <span>
#include "shared_memory.h"
#include "../constants/combat_constants.h"

std::uint32_t read_team_header() {
    std::array<std::byte, 4> raw{};
    cambc::shared_memory.read(team_comms::kHeaderByte, std::span{raw});
    std::array<std::uint8_t, 4> b{};
    for (int i = 0; i < 4; ++i) b[i] = std::to_integer<std::uint8_t>(raw[i]);
    return team_comms::word_from_bytes(b);
}

void write_team_header(std::uint32_t word) {
    const auto b = team_comms::bytes_from_word(word);
    std::array<std::byte, 4> raw{};
    for (int i = 0; i < 4; ++i) raw[i] = std::byte(b[i]);
    cambc::shared_memory.write(team_comms::kHeaderByte, std::span{raw});
}

void sweep_team_header() {
    using namespace team_comms;
    static_assert(kFarmTargetRounds < (1 << kFarmRoundBits) - 1, "farm lifetime must fit the round counter");
    static_assert(constants::kTeamFightCallRounds < (1 << kCallRoundBits) - 1, "call lifetime must fit the round counter");
    const auto round = cambc::round_number();
    const auto word = read_team_header();
    auto w = word;
    const auto farm = get_farm(w);
    if (farm.group_plus1 != 0 && age(round, farm.round_low, kFarmRoundBits) > kFarmTargetRounds) w = clear_farm(w);
    const auto call = get_call(w);
    if (call.enemy_plus1 != 0 && age(round, call.round_low, kCallRoundBits) > constants::kTeamFightCallRounds) w = clear_call(w);
    if (w != word) write_team_header(w);
}
