#include "group_routes.h"
#include <algorithm>

namespace group_routes {
cambc::vec2 group_centre(const cambc::map::camp_group& group) {
    const auto positions = group.spawn_tiles().positions();
    return positions[positions.size() / 2];
}

cambc::bitboard route_walls(const std::vector<cambc::map::camp_group>& groups) {
    cambc::bitboard camp_tiles;
    camp_tiles.clear();
    for (const auto& g : groups) camp_tiles |= g.spawn_tiles();
    const auto blockers = cambc::map::walls() | cambc::map::holes() | cambc::map::altars()
        | cambc::map::spawn_region(cambc::team::enemy);
    return blockers & ~camp_tiles;
}

void compute_row(Table& table, int i, const cambc::bitboard& walls, const std::vector<cambc::vec2>& centres) {
    cambc::distance_field field;
    cambc::bitboard visited;
    cambc::bitboard root;
    root.clear();
    root.set(centres[static_cast<std::size_t>(i)]);
    field.compute(walls, root, visited, static_cast<std::uint16_t>(kMaxWalk));
    for (int j = i + 1; j < table.n; ++j) {
        const auto d = field.get(centres[static_cast<std::size_t>(j)]);
        const std::uint8_t v = d > kMaxWalk ? kFar : static_cast<std::uint8_t>(d);
        table.dist[static_cast<std::size_t>(i * table.n + j)] = v;
        table.dist[static_cast<std::size_t>(j * table.n + i)] = v;
    }
    table.rows_done = std::max(table.rows_done, i + 1);
}

std::vector<std::uint8_t> pack(const Table& table) {
    std::vector<std::uint8_t> out{kMagic, static_cast<std::uint8_t>(table.n), static_cast<std::uint8_t>(table.rows_done)};
    for (int i = 0; i < table.rows_done; ++i)
        for (int j = i + 1; j < table.n; ++j) out.push_back(table.dist[static_cast<std::size_t>(i * table.n + j)]);
    return out;
}

bool unpack(const cambc::draft_memory_buffer& memory, int n_groups, Table& table) {
    if (memory.size() < kHeader) return false;
    const auto byte = [&](std::size_t k) { return std::to_integer<std::uint8_t>(memory[k]); };
    if (byte(0) != kMagic || byte(1) != n_groups || n_groups > kMaxGroups) return false;
    table.n = n_groups;
    table.dist.assign(static_cast<std::size_t>(n_groups * n_groups), kFar);
    const int rows = std::min<int>(byte(2), n_groups);
    std::size_t k = kHeader;
    for (int i = 0; i < rows; ++i)
        for (int j = i + 1; j < n_groups; ++j, ++k) {
            if (k >= memory.size()) return false;
            const auto v = byte(k);
            table.dist[static_cast<std::size_t>(i * n_groups + j)] = v;
            table.dist[static_cast<std::size_t>(j * n_groups + i)] = v;
        }
    table.rows_done = rows;
    return true;
}
}  // namespace group_routes
