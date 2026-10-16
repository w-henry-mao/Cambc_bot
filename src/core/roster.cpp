#include "roster.h"
#include <deque>
const std::vector<cambc::champion>& roster_allies() {
    static const auto allies = cambc::champions(cambc::team::ally);
    return allies;
}
const std::vector<cambc::champion>& roster_enemies() {
    static const auto enemies = cambc::champions(cambc::team::enemy);
    return enemies;
}
bool in_view(const cambc::champion& c) { return c.is_alive() && c.is_visible() && c.health() > 0; }
const cambc::champion& ability_view(const cambc::champion& c) {
    static std::deque<cambc::champion> views;   // deque: references stay valid as it grows
    for (const auto& v : views)
        if (v == c) return v;
    views.push_back(cambc::vision::champion_at(c.position()));   // built now, while visible
    return views.back();
}
