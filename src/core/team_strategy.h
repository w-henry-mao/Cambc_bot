#pragma once

#include <cambc.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <span>
#include <vector>

namespace team_strategy {

// Shared bytes 0-3: mode (3 bits), Prism position (2 x 7 bits), target (2 x 7 bits).
// Shared bytes 4-7: final-tribute phase (2 bits), altar (2 x 7 bits), five 2-bit
// readiness statuses, and five recall acknowledgements. The fields do not overlap.
// Coordinates use 7 bits because the largest supported maps are 100 x 100.
namespace layout {
inline constexpr std::uint32_t field_mask(unsigned offset, unsigned count) {
    return ((std::uint32_t{1} << count) - 1u) << offset;
}

inline constexpr std::size_t kGeneralWordByteOffset = 0;
inline constexpr std::size_t kFinalTributeWordByteOffset = 4;

inline constexpr unsigned kGeneralModeOffset = 0;
inline constexpr unsigned kPrismPositionOffset = 3;
inline constexpr unsigned kTargetPositionOffset = 17;
inline constexpr auto kGeneralModeMask = field_mask(kGeneralModeOffset, 3);
inline constexpr auto kPrismPositionMask = field_mask(kPrismPositionOffset, 14);
inline constexpr auto kTargetPositionMask = field_mask(kTargetPositionOffset, 14);

inline constexpr unsigned kFinalTributeModeOffset = 0;
inline constexpr unsigned kAltarXOffset = 2;
inline constexpr unsigned kAltarYOffset = 9;
inline constexpr unsigned kFinalTributeStatusesOffset = 16;
inline constexpr unsigned kFinalTributeStatusBits = 2;
inline constexpr unsigned kRecallAcksOffset = 26;
inline constexpr unsigned kTeamSlots = 5;

inline constexpr auto kFinalTributePhaseMask = field_mask(kFinalTributeModeOffset, 2);
inline constexpr auto kAltarXMask = field_mask(kAltarXOffset, 7);
inline constexpr auto kAltarYMask = field_mask(kAltarYOffset, 7);
inline constexpr auto kFinalTributeStatusesMask =
    field_mask(kFinalTributeStatusesOffset, kFinalTributeStatusBits * kTeamSlots);
inline constexpr auto kRecallAcksMask = field_mask(kRecallAcksOffset, kTeamSlots);
inline constexpr auto kFinalTributeBlockMask = kFinalTributePhaseMask | kAltarXMask | kAltarYMask |
    kFinalTributeStatusesMask | kRecallAcksMask;

static_assert(kGeneralWordByteOffset + 4 <= kFinalTributeWordByteOffset);
static_assert((kGeneralModeMask & kPrismPositionMask) == 0);
static_assert((kGeneralModeMask & kTargetPositionMask) == 0);
static_assert((kPrismPositionMask & kTargetPositionMask) == 0);
static_assert((kGeneralModeMask | kPrismPositionMask | kTargetPositionMask) == 0x7fffffffu);
static_assert((kFinalTributePhaseMask & kAltarXMask) == 0);
static_assert((kFinalTributePhaseMask & kAltarYMask) == 0);
static_assert((kFinalTributePhaseMask & kFinalTributeStatusesMask) == 0);
static_assert((kFinalTributePhaseMask & kRecallAcksMask) == 0);
static_assert((kAltarXMask & kAltarYMask) == 0);
static_assert((kAltarXMask & kFinalTributeStatusesMask) == 0);
static_assert((kAltarXMask & kRecallAcksMask) == 0);
static_assert((kAltarYMask & kFinalTributeStatusesMask) == 0);
static_assert((kAltarYMask & kRecallAcksMask) == 0);
static_assert((kFinalTributeStatusesMask & kRecallAcksMask) == 0);
static_assert(kFinalTributeBlockMask == 0x7fffffffu);
} // namespace layout

enum class Mode : std::uint8_t {
    independent = 0,
    farming = 1,
    combat = 2,
    recalling = 3,
    recalling_home = 4,
    arrived = 5,
    prism_dead = 6,
    farming_unleashed = 7,
};

struct Command {
    Mode mode = Mode::independent;
    cambc::vec2 prism_position{127, 127};
    cambc::vec2 target_position{127, 127};
};

inline std::uint32_t read_word(std::size_t first_byte) {
    std::array<std::byte, 4> bytes{};
    cambc::shared_memory.read(first_byte, std::span<std::byte>{bytes});
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[i])) << (8 * i);
    return value;
}

inline void write_word(std::size_t first_byte, std::uint32_t value) {
    std::array<std::byte, 4> bytes{};
    for (std::size_t i = 0; i < 4; ++i)
        bytes[i] = static_cast<std::byte>((value >> (8 * i)) & 0xffu);
    cambc::shared_memory.write(first_byte, std::span<const std::byte>{bytes});
}

inline std::uint32_t get_bits(std::uint32_t word, unsigned offset, unsigned count) {
    return (word >> offset) & ((1u << count) - 1u);
}

inline void set_bits(std::uint32_t& word, unsigned offset, unsigned count, std::uint32_t value) {
    const auto mask = ((1u << count) - 1u) << offset;
    word = (word & ~mask) | ((value << offset) & mask);
}

inline std::uint32_t encode_position(cambc::vec2 position) {
    const auto x = (position.x >= 0 && position.x < 100) ? static_cast<std::uint32_t>(position.x) : 127u;
    const auto y = (position.y >= 0 && position.y < 100) ? static_cast<std::uint32_t>(position.y) : 127u;
    return x | (y << 7);
}

inline cambc::vec2 decode_position(std::uint32_t packed) {
    return {static_cast<std::int32_t>(packed & 0x7fu), static_cast<std::int32_t>((packed >> 7) & 0x7fu)};
}

inline bool valid_position(cambc::vec2 position) {
    return position.x >= 0 && position.x < 100 && position.y >= 0 && position.y < 100;
}

inline Command read() {
    const auto word = read_word(0);
    Command command;
    command.mode = static_cast<Mode>(get_bits(word, layout::kGeneralModeOffset, 3));
    command.prism_position = decode_position(get_bits(word, layout::kPrismPositionOffset, 14));
    command.target_position = decode_position(get_bits(word, layout::kTargetPositionOffset, 14));
    return command;
}

inline void publish(Mode mode, cambc::vec2 prism_position,
                    std::optional<cambc::vec2> target = std::nullopt) {
    std::uint32_t word = 0;
    set_bits(word, layout::kGeneralModeOffset, 3, static_cast<std::uint8_t>(mode));
    set_bits(word, layout::kPrismPositionOffset, 14, encode_position(prism_position));
    set_bits(word, layout::kTargetPositionOffset, 14, encode_position(target.value_or(cambc::vec2{127, 127})));
    write_word(layout::kGeneralWordByteOffset, word);
}

inline std::vector<cambc::champion> ordered_allies() {
    auto allies = cambc::champions(cambc::team::ally);
    std::sort(allies.begin(), allies.end(), [](const auto& a, const auto& b) {
        return static_cast<int>(a.champion_name()) < static_cast<int>(b.champion_name());
    });
    return allies;
}

inline std::optional<std::uint8_t> self_slot() {
    const auto allies = ordered_allies();
    const auto self_type = cambc::self().champion_name();
    for (std::size_t i = 0; i < allies.size() && i < 5; ++i)
        if (allies[i].champion_name() == self_type) return static_cast<std::uint8_t>(i);
    return std::nullopt;
}

inline std::uint8_t recall_ack_mask() {
    return static_cast<std::uint8_t>(get_bits(read_word(layout::kFinalTributeWordByteOffset),
                                                layout::kRecallAcksOffset, layout::kTeamSlots));
}

inline void clear_recall_ack_mask() {
    auto word = read_word(layout::kFinalTributeWordByteOffset);
    set_bits(word, layout::kRecallAcksOffset, layout::kTeamSlots, 0);
    write_word(layout::kFinalTributeWordByteOffset, word);
}

inline void acknowledge_recall_arrival(std::uint8_t slot) {
    if (slot >= layout::kTeamSlots) return;
    auto word = read_word(layout::kFinalTributeWordByteOffset);
    const auto mask = get_bits(word, layout::kRecallAcksOffset, layout::kTeamSlots) | (1u << slot);
    set_bits(word, layout::kRecallAcksOffset, layout::kTeamSlots, mask);
    write_word(layout::kFinalTributeWordByteOffset, word);
}

inline std::uint8_t required_recall_ack_mask() {
    std::uint8_t mask = 0;
    const auto allies = ordered_allies();
    for (std::size_t i = 0; i < allies.size() && i < layout::kTeamSlots; ++i)
        if (allies[i].is_alive() && allies[i].champion_name() != cambc::champion_type::prism)
            mask |= static_cast<std::uint8_t>(1u << i);
    return mask;
}

} // namespace team_strategy
