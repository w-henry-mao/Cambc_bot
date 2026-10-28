#include "our_policy.h"

bool is_offensive(const AbilityDesign& a) {
    return false;
}

bool is_support(const AbilityDesign& a) {
    return false;
}

int offensive_slot(const PolicyView& v) {
    return -1;
}

int support_slot_self(const PolicyView& v) {
    return -1;
}

int reset_slot(const PolicyView& v) {
    return -1;
}

int reach(const ChampionDesign& d) {
    return 0;
}

int threat_reach(const ChampionDesign& d) {
    return 0;
}

bool can_hit(const PolicyView& v) {
    return false;
}
