#pragma once
#include <array>
#include <cstdint>
struct DraftMemory {
    std::uint32_t version = 1;
    std::array<std::uint8_t, 1020> reserved{};
    std::array<std::uint8_t, 1024>& buffer() {
        return reinterpret_cast<std::array<std::uint8_t, 1024>&>(*this);
    }
};
