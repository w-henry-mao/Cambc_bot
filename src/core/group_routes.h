#pragma once
#include <cambc.h>
#include <cstddef>
#include <cstdint>
#include <vector>
// Walking distances between camp group centres (camp tiles count as walkable, as in the farm
// state's own estimate), for route planning. Computed in the draft, where fuel is spare, and
// passed to the champions in draft memory; rows the draft did not finish are computed in the
// deferred setup. One BFS per group, capped at kMaxWalk steps.
//
// Draft memory layout: byte 0 kMagic, byte 1 group count n, byte 2 rows done, then row i's
// distances to groups i+1..n-1 for each finished row (upper triangle), one byte each.
namespace group_routes {
constexpr std::uint8_t kMagic = 0xA7;
constexpr std::uint8_t kFar = 255;      // unreachable within kMaxWalk
constexpr int kMaxWalk = 120;
constexpr std::size_t kHeader = 3;
// Largest n whose triangle fits the 1024-byte draft memory.
constexpr int kMaxGroups = 45;

struct Table {
    int n = 0;
    int rows_done = 0;                  // rows 0..rows_done-1 filled (both halves)
    std::vector<std::uint8_t> dist;     // n x n, kFar where unknown
    [[nodiscard]] bool ready() const { return n > 0 && rows_done >= n - 1; }
    [[nodiscard]] int get(int i, int j) const {
        return i == j ? 0 : dist[static_cast<std::size_t>(i * n + j)];
    }
};

// The same centre the map snapshot uses: the middle spawn tile.
cambc::vec2 group_centre(const cambc::map::camp_group& group);
// Walls for the estimate: static blockers minus every group's spawn tiles.
cambc::bitboard route_walls(const std::vector<cambc::map::camp_group>& groups);
// Fills row `i` (distances from group i to every later group, both halves of the table).
void compute_row(Table& table, int i, const cambc::bitboard& walls, const std::vector<cambc::vec2>& centres);
// Packs the finished rows for draft memory; returns the bytes to submit.
std::vector<std::uint8_t> pack(const Table& table);
// Reads a packed table; false (table untouched) if absent or for a different group count.
bool unpack(const cambc::draft_memory_buffer& memory, int n_groups, Table& table);
}  // namespace group_routes
