#include "team_comms.h"
#include <array>
#include <cambc.h>
#include <cstddef>
#include <span>
#include "shared_memory.h"
#include "../constants/combat_constants.h"
#include "../constants/state_constants.h"

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
    if constexpr (constants::kSplitGroups) return;   // bytes 6-7 hold the second group's target
    // Rally point: clear once expired so its 4-bit round stamp cannot wrap into looking fresh.
    std::array<std::byte, 2> raw{};
    cambc::shared_memory.read(kRallyByte, std::span{raw});
    const auto bits = static_cast<std::uint16_t>(std::to_integer<unsigned>(raw[0]) | (std::to_integer<unsigned>(raw[1]) << 8));
    const auto rally = unpack_rally(bits);
    if (rally.set && age(round, rally.round_low, kRallyRoundBits) > constants::kRallyRounds) {
        const std::array<std::byte, 2> zero{};
        cambc::shared_memory.write(kRallyByte, std::span{zero});
    }
}

void write_rally(int x, int y) {
    const auto bits = team_comms::pack_rally(x, y, cambc::round_number());
    const std::array<std::byte, 2> raw{std::byte(static_cast<unsigned char>(bits & 0xff)), std::byte(static_cast<unsigned char>(bits >> 8))};
    cambc::shared_memory.write(team_comms::kRallyByte, std::span{raw});
}

team_comms::RallyField read_rally(int max_age) {
    std::array<std::byte, 2> raw{};
    cambc::shared_memory.read(team_comms::kRallyByte, std::span{raw});
    const auto bits = static_cast<std::uint16_t>(std::to_integer<unsigned>(raw[0]) | (std::to_integer<unsigned>(raw[1]) << 8));
    auto r = team_comms::unpack_rally(bits);
    if (r.set && team_comms::age(cambc::round_number(), r.round_low, team_comms::kRallyRoundBits) > max_age) r.set = false;
    return r;
}

void write_group2_target(int group_plus1) {
    const std::array<std::byte, 2> raw{std::byte(static_cast<unsigned char>(group_plus1)),
                                       std::byte(static_cast<unsigned char>(cambc::round_number() & 0xff))};
    cambc::shared_memory.write(team_comms::kRallyByte, std::span{raw});
}

int read_group2_target(int max_age) {
    std::array<std::byte, 2> raw{};
    cambc::shared_memory.read(team_comms::kRallyByte, std::span{raw});
    const int g = std::to_integer<int>(raw[0]);
    const int age = static_cast<int>((cambc::round_number() - std::to_integer<unsigned>(raw[1])) & 0xff);
    return g != 0 && age <= max_age ? g : 0;
}
