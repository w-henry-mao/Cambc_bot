#pragma once
#include <array>
#include <cstdint>
struct SharedMemory {
    std::array<std::uint8_t, 8> data{};
    std::array<std::uint8_t, 8>& buffer() { return data; }
};
