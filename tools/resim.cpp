// Host debugging tool: re-run the fight model from a FIGHT_PRED/FIGHT_TURN setup and print every
// turn. Build: clang++ -std=c++20 -I src tools/resim.cpp src/combat/{simulator,turn_order,our_policy}.cpp -o resim
// Run:   ./resim "S_us=10,1000,... S_en=... S_d=3 S_acted=0"
#include <cstdio>
#include <cstring>
#include <string>
#include "combat/design_table.h"
#include "combat/simulator.h"

static Fighter parse_fighter(const std::string& line, const char* tag) {
    Fighter f;
    const auto at = line.find(std::string(tag) + "=");
    int type = 0;
    std::sscanf(line.c_str() + at + std::strlen(tag) + 1, "%d,%f,%f,%f,%f,%f,%d,%d,%d,%d,%f", &type, &f.health, &f.max_health,
                &f.strength, &f.focus, &f.shield, &f.shield_turns, &f.cooldown[0], &f.cooldown[1], &f.stunned_rounds,
                &f.damage_taken_mult);
    f.design = &design_for(type);
    return f;
}
static void print_turn(int t, bool ours, const Fighter& us, const Fighter& en, int d) {
    std::printf("t=%2d %s d=%d | us hp=%6.1f sh=%5.1f cd=%d,%d st=%d | en hp=%6.1f sh=%5.1f cd=%d,%d st=%d\n", t,
                ours ? "US" : "EN", d, us.health, us.shield, us.cooldown[0], us.cooldown[1], us.stunned_rounds, en.health,
                en.shield, en.cooldown[0], en.cooldown[1], en.stunned_rounds);
}
int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: resim \"<setup fields>\"\n"); return 2; }
    const std::string line = argv[1];
    DuelSetup s;
    s.us = parse_fighter(line, "S_us");
    s.enemy = parse_fighter(line, "S_en");
    std::sscanf(line.c_str() + line.find("S_d=") + 4, "%d", &s.walk_distance);
    s.enemy_acted_this_round = line.find("S_acted=1") != std::string::npos;
    s.same_turn_position = line.find("S_same=0") == std::string::npos;
    FightOptions o;
    o.on_turn = print_turn;
    const auto r = simulate_duel_fast(s, o);
    std::printf("result: win=%d our_hp=%g enemy_hp=%g turns=%d resolved=%d\n", r.we_win, r.our_health_worst_case,
                r.enemy_health_at_end, r.turns, r.resolved);
}
