#include "group_routes.h"

namespace group_routes {
cambc::vec2 group_centre(const cambc::map::camp_group& group) {
    return cambc::vec2{0, 0};
}

cambc::bitboard route_walls(const std::vector<cambc::map::camp_group>& groups) {
    cambc::bitboard empty;
    empty.clear();
    return empty;
}

void compute_row(Table& table, int i, const cambc::bitboard& walls, const std::vector<cambc::vec2>& centres) {
}

std::vector<std::uint8_t> pack(const Table& table) {
    return std::vector<std::uint8_t>{};
}

bool unpack(const cambc::draft_memory_buffer& memory, int n_groups, Table& table) {
    return false;
}
}  // namespace group_routes
