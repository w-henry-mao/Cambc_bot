// Test-only: print every champion's design data once, then resign.
#include <cambc.h>
#include <iostream>
#include <variant>
#include "draft.h"
namespace bc = cambc;
static void dump_ability(const bc::ability& ab) {
    const auto& info = ab.info();
    if (!info) { std::cout << "{\"kind\":\"none\"}"; return; }
    std::visit([&](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        float range = 0, modifier = 0, radius = 0; int duration = 0; bool enhanced = false; const char* kind = "?";
        if constexpr (std::is_same_v<T, bc::ability_attack_reset>) { kind = "attack_reset"; enhanced = p.enhanced_attack; }
        if constexpr (std::is_same_v<T, bc::ability_damage>) { kind = "damage"; range = p.range; modifier = p.modifier; }
        if constexpr (std::is_same_v<T, bc::ability_aoe_damage>) { kind = "aoe_damage"; range = p.range; modifier = p.modifier; radius = p.radius; duration = p.duration; }
        if constexpr (std::is_same_v<T, bc::ability_cc>) { kind = "cc"; range = p.range; modifier = p.modifier; duration = p.duration; }
        if constexpr (std::is_same_v<T, bc::ability_heal>) { kind = "heal"; range = p.range; modifier = p.modifier; }
        if constexpr (std::is_same_v<T, bc::ability_dash>) { kind = "dash"; range = p.range; }
        if constexpr (std::is_same_v<T, bc::ability_shield>) { kind = "shield"; range = p.range; modifier = p.modifier; duration = p.duration; }
        const auto t = p.valid_targets;
        std::cout << "{\"kind\":\"" << kind << "\",\"range\":" << range << ",\"modifier\":" << modifier
                  << ",\"radius\":" << radius << ",\"duration\":" << duration << ",\"cooldown\":" << p.max_cooldown
                  << ",\"excludes_walking\":" << p.exclusive_with_movement << ",\"enhanced\":" << enhanced
                  << ",\"enemies\":" << t.contains(bc::ability_target_mask::enemies)
                  << ",\"self\":" << t.contains(bc::ability_target_mask::self)
                  << ",\"allies\":" << t.contains(bc::ability_target_mask::allies)
                  << ",\"camps\":" << t.contains(bc::ability_target_mask::camps)
                  << ",\"ground\":" << t.contains(bc::ability_target_mask::ground) << "}";
    }, *info);
}
int main() {
    int next = 0;
    while (true) {
        try {
            if (bc::is_draft()) play_draft();
            else if (next < 12) {
                {
                    const int i = next++;
                    bc::champion c{static_cast<bc::champion_type>(i)};
                    const auto s = c.base_stats();
                    std::cout << "DESIGN {\"type\":" << i << ",\"name\":\"" << bc::to_string(c.champion_name())
                              << "\",\"passive\":" << static_cast<int>(c.passive())
                              << ",\"vision\":" << s.vision_radius << ",\"move\":" << s.base_move_speed
                              << ",\"attack_range\":" << s.base_attack_range
                              << ",\"health\":" << s.base_health << ",\"health_growth\":" << s.health_growth
                              << ",\"strength\":" << s.base_strength << ",\"strength_growth\":" << s.strength_growth
                              << ",\"focus\":" << s.base_focus << ",\"focus_growth\":" << s.focus_growth
                              << ",\"camp_mult\":" << s.camp_damage_multiplier << ",\"slots\":[";
                    const auto abs = c.abilities();
                    dump_ability(abs[0]); std::cout << ","; dump_ability(abs[1]);
                    std::cout << "]}\n";
                }
                std::cout.flush();
            }
        } catch (const std::exception& e) { std::cout << "bot error: " << e.what() << std::endl; }
        bc::end_turn();
    }
}
