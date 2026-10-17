#include "test.h"
#include "comms/shared_memory.h"

using namespace team_comms;

TEST(test_header_fields_round_trip_and_stay_separate) {
    for (int g = 0; g <= kMaxFarmGroups; g += 7)
        for (std::uint32_t r = 0; r < 300; r += 37)
            for (int e = 0; e <= 7; ++e)
                for (int x = 0; x <= kMaxCoord; x += 31) {
                    const int y = kMaxCoord - x;
                    std::uint32_t w = 0;
                    w = set_farm(w, g, r);
                    w = set_call(w, e, r + 5, x, y);
                    const auto f = get_farm(w);
                    const auto c = get_call(w);
                    CHECK(f.group_plus1 == g); CHECK(f.round_low == static_cast<int>(r & 7));
                    CHECK(c.enemy_plus1 == e); CHECK(c.round_low == static_cast<int>((r + 5) & 63));
                    CHECK(c.x == x); CHECK(c.y == y);
                    // Rewriting one field leaves the other intact.
                    const auto w2 = set_farm(w, (g + 1) % (kMaxFarmGroups + 1), r + 1);
                    CHECK(get_call(w2).enemy_plus1 == e); CHECK(get_call(w2).x == x); CHECK(get_call(w2).y == y);
                    const auto w3 = set_call(w, 0, 0, 0, 0);
                    CHECK(get_farm(w3).group_plus1 == g);
                    CHECK(get_farm(clear_call(w)).group_plus1 == g);
                    CHECK(get_call(clear_farm(w)).enemy_plus1 == e);
                }
}
TEST(test_header_bytes_and_limits) {
    const std::uint32_t w = set_call(set_farm(0, 42, 1001), 6, 1002, 100, 99);
    CHECK(word_from_bytes(bytes_from_word(w)) == w);
    // Out-of-range values never spill into other fields.
    const auto big = set_call(set_farm(0, 99, 3), 9, 3, 500, -4);
    CHECK(get_farm(big).group_plus1 == 0);
    CHECK(get_call(big).enemy_plus1 == 0); CHECK(get_call(big).x == kMaxCoord); CHECK(get_call(big).y == 0);
}
TEST(test_header_age) {
    // Ages count across counter wrap-around.
    CHECK(age(1000, static_cast<int>(1000 & 7), kFarmRoundBits) == 0);
    CHECK(age(1002, static_cast<int>(1000 & 7), kFarmRoundBits) == 2);
    CHECK(age(1003, static_cast<int>(1000 & 7), kFarmRoundBits) == 3);
    CHECK(age(64 + 2, 63, kCallRoundBits) == 3);
    CHECK(age(5, 5, kCallRoundBits) == 0);
}
TEST(test_layout_bytes) {
    CHECK(kHeaderByte + 4 == kPositionByte); CHECK(kPositionByte + 2 == kFreeByte); CHECK(kFreeByte + 2 == 8);
}
