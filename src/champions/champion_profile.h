#pragma once
#include <cambc.h>
#include <functional>
#include <vector>
class Skill;
struct ChampionProfile {
    bool is_ranged;
    std::vector<Skill*> skills;
    bool linked_pair;
    std::function<void()> special_hook = []{};
};
