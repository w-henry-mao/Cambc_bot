#pragma once
#include <cambc.h>
#include <cstdint>
#include <random>
inline std::uint32_t random_seed() {
    const auto spawn = cambc::map::spawn_region(cambc::team::ally).positions()[0];
    std::uint32_t seed = spawn.x * 73856093u ^ spawn.y * 19349663u;
    if (!cambc::is_draft()) {
        const auto start = cambc::self().position();
        seed ^= start.x * 83492791u ^ start.y * 2654435761u;
    }
    return seed;
}
inline unsigned random_number(unsigned limit) {
    static std::mt19937 random{random_seed()};
    return std::uniform_int_distribution<unsigned>{0, limit - 1}(random);
}
