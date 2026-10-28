#include "roster.h"

const std::vector<cambc::champion>& roster_allies() {
    static std::vector<cambc::champion> empty;
    return empty;
}

const std::vector<cambc::champion>& roster_enemies() {
    static std::vector<cambc::champion> empty;
    return empty;
}

bool in_view(const cambc::champion& c) {
    return false;
}

const cambc::champion& ability_view(const cambc::champion& c) {
    return c;
}
