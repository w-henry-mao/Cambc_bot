#include "simulator.h"
#include <algorithm>
#include <vector>
#include "our_policy.h"
#include "turn_order.h"

namespace {
constexpr float kRetaliationFocusRatio = 0.2f;   // Ren, docs/combat-rules.md #6b

struct State {
    Fighter us, en;
    int d = 1;
};

// What the acting champion may still do this turn (docs/combat-rules.md #2b-#4b).
struct TurnCtx {
    bool move = true, attack = true, cast = true;
    bool attack_spent = false;   // a reset needs the attack used first
    bool enhanced = false;       // next attack stuns (Marcella's enhanced reset)
};

bool dead(const Fighter& f) { return f.health <= 0; }

void take(Fighter& t, float amount) {
    amount *= t.damage_taken_mult;
    const float absorbed = std::min(t.shield, amount);
    t.shield -= absorbed;
    t.health -= amount - absorbed;
}

// Damage from `src` to `tgt`, with Ren's retaliation back onto `src`.
void hit(Fighter& tgt, Fighter& src, float amount) {
    if (amount <= 0) return;
    take(tgt, amount);
    if (tgt.design->passive == Passive::Retaliation) take(src, kRetaliationFocusRatio * tgt.focus);
}

void stun(Fighter& t, int rounds) {
    if (rounds <= 0 || t.design->passive == Passive::StunImmunity) return;
    t.stunned_rounds = std::max(t.stunned_rounds, rounds);
}

void end_turn(Fighter& f) {
    for (int& c : f.cooldown) c = std::max(0, c - 1);
    if (f.shield_turns > 0) --f.shield_turns;
}

// Zero timers resolve when the owner's turn starts: a shield with no turns left goes now.
void expire_shield(Fighter& f) {
    if (f.shield > 0 && f.shield_turns == 0) f.shield = 0;
}

bool in_range(const State& s, float range) { return static_cast<float>(s.d) <= range; }

bool walk(State& s, Fighter& a, TurnCtx& c) {
    if (!c.move) return false;
    s.d = std::max(1, s.d - 1);
    c.move = false;
    if (a.design->ranged) { c.attack = false; c.attack_spent = true; }   // ranged: walking uses the attack (a reset restores it)
    return true;
}

bool attack(State& s, Fighter& a, Fighter& t, TurnCtx& c) {
    if (!c.attack || !in_range(s, static_cast<float>(a.design->attack_range))) return false;
    hit(t, a, a.strength);
    if (c.enhanced) { stun(t, 1); c.enhanced = false; }
    c.attack = false;
    c.attack_spent = true;
    if (a.design->ranged) c.move = false;     // ranged: attacking uses the move
    return true;
}

bool can_cast(const Fighter& a, const TurnCtx& c, int slot) {
    const auto& ab = a.design->slots[slot];
    return c.cast && ab.kind != AbilityKind::None && a.cooldown[slot] == 0 && (!ab.excludes_walking || c.move);
}

void start_cast(Fighter& a, TurnCtx& c, int slot) {
    const auto& ab = a.design->slots[slot];
    c.cast = false;
    a.cooldown[slot] = ab.cooldown;
    if (ab.excludes_walking) c.move = false;
}

// Offensive spell on the target: damage = modifier x Focus; stuns when it has a duration.
bool cast_offensive(State& s, Fighter& a, Fighter& t, TurnCtx& c, int slot) {
    const auto& ab = a.design->slots[slot];
    if (!can_cast(a, c, slot) || !is_offensive(ab) || !in_range(s, ab.range)) return false;
    start_cast(a, c, slot);
    hit(t, a, ab.modifier * a.focus);
    if (ab.kind == AbilityKind::Cc || ab.kind == AbilityKind::AoeDamage) stun(t, ab.duration);
    return true;
}

bool cast_shield(Fighter& a, TurnCtx& c, int slot) {
    const auto& ab = a.design->slots[slot];
    if (!can_cast(a, c, slot) || ab.kind != AbilityKind::Shield) return false;
    start_cast(a, c, slot);
    a.shield = ab.modifier * a.focus;
    a.shield_turns = ab.duration;
    return true;
}

bool cast_reset(Fighter& a, TurnCtx& c, int slot) {
    const auto& ab = a.design->slots[slot];
    if (!can_cast(a, c, slot) || ab.kind != AbilityKind::AttackReset || !c.attack_spent || c.attack) return false;
    start_cast(a, c, slot);
    c.attack = true;
    if (ab.enhanced_attack) c.enhanced = true;
    return true;
}

bool cast_dash(State& s, Fighter& a, TurnCtx& c, int slot) {
    const auto& ab = a.design->slots[slot];
    if (!can_cast(a, c, slot) || ab.kind != AbilityKind::Dash) return false;
    start_cast(a, c, slot);
    s.d = std::max(1, s.d - static_cast<int>(ab.range));
    return true;
}

PolicyView view(const State& s, const Fighter& a, const TurnCtx& c) {
    PolicyView v;
    v.design = a.design;
    v.distance = s.d;
    v.can_move = c.move; v.can_attack = c.attack; v.can_cast = c.cast;
    v.cooldown[0] = a.cooldown[0]; v.cooldown[1] = a.cooldown[1];
    v.health_fraction = a.max_health > 0 ? a.health / a.max_health : 0;
    return v;
}

// Mirrors skills/casting.cpp attack_with_reset: attack, then reset and attack again.
bool attack_with_reset(State& s, Fighter& a, Fighter& t, TurnCtx& c) {
    if (!can_hit(view(s, a, c)) || !attack(s, a, t, c)) return false;
    if (dead(t) || dead(a)) return true;
    const int r = reset_slot(view(s, a, c));
    if (r >= 0 && cast_reset(a, c, r)) attack(s, a, t, c);
    return true;
}

// Our side, in the order of states/attack.cpp: support, offensive cast, attack with reset,
// otherwise close in and attack.
void our_turn(State& s) {
    TurnCtx c;
    Fighter& a = s.us;
    Fighter& t = s.en;
    // The attack state only runs with the enemy within reach; otherwise the bot is busy with
    // something else (farming) and does nothing in this fight.
    if (s.d > reach(*a.design)) return;
    const int sup = support_slot_self(view(s, a, c));
    if (sup >= 0) cast_shield(a, c, sup);
    const int off = offensive_slot(view(s, a, c));
    if (off >= 0) cast_offensive(s, a, t, c, off);
    if (dead(a) || dead(t)) return;
    if (!attack_with_reset(s, a, t, c) && c.move && kAttackStateClosesIn) {
        walk(s, a, c);
        attack_with_reset(s, a, t, c);
    }
}


void close_and_attack(State& s, Fighter& a, Fighter& t, TurnCtx& c) {
    if (!in_range(s, static_cast<float>(a.design->attack_range))) walk(s, a, c);
    attack(s, a, t, c);
}

void enemy_turn(State& s, const Plan& p) {
    TurnCtx c;
    Fighter& a = s.en;
    Fighter& t = s.us;
    switch (p.kind) {
        case PlanKind::Plain: close_and_attack(s, a, t, c); break;
        case PlanKind::Offensive: {
            const auto& ab = a.design->slots[p.slot];
            if (!in_range(s, ab.range) && !ab.excludes_walking) walk(s, a, c);
            cast_offensive(s, a, t, c, p.slot);
            if (!dead(t) && !dead(a)) close_and_attack(s, a, t, c);
            break;
        }
        case PlanKind::Reset:
            close_and_attack(s, a, t, c);
            if (!dead(t) && !dead(a) && cast_reset(a, c, p.slot)) attack(s, a, t, c);
            break;
        case PlanKind::Dash:
            cast_dash(s, a, c, p.slot);
            close_and_attack(s, a, t, c);
            break;
        case PlanKind::Shield:
            cast_shield(a, c, p.slot);
            close_and_attack(s, a, t, c);
            break;
    }
}

struct PlanList { Plan items[5]; int count = 0; void add(Plan p) { items[count++] = p; } };

PlanList enemy_plans(const State& s) {
    PlanList plans;
    plans.add({PlanKind::Plain, -1});
    const Fighter& a = s.en;
    for (int i = 0; i < 2; ++i) {
        const auto& ab = a.design->slots[i];
        if (a.cooldown[i] != 0) continue;
        switch (ab.kind) {
            case AbilityKind::Damage: case AbilityKind::AoeDamage: case AbilityKind::Cc:
                if (ab.targets_enemies && (in_range(s, ab.range) || (!ab.excludes_walking && s.d - 1 <= ab.range)))
                    plans.add({PlanKind::Offensive, i});
                break;
            case AbilityKind::AttackReset: plans.add({PlanKind::Reset, i}); break;
            case AbilityKind::Dash: if (s.d > 1) plans.add({PlanKind::Dash, i}); break;
            case AbilityKind::Shield: plans.add({PlanKind::Shield, i}); break;
            default: break;
        }
    }
    return plans;
}


// Nothing but basic attacks can happen this turn: both in attack range and every ability
// either side would use is on cooldown. (Same outcome as the full turn, less work.)
bool usable_by_us(const AbilityDesign& a) {
    return is_offensive(a) || a.kind == AbilityKind::AttackReset || (is_support(a) && a.targets_self);
}
bool usable_by_enemy(const AbilityDesign& a) {
    return is_offensive(a) || a.kind == AbilityKind::AttackReset || a.kind == AbilityKind::Shield;
}
// A ready self-support (Varg's shield) waits until we drop below kSupportBelow.
bool support_waiting(const Fighter& f, int slot) {
    const auto& a = f.design->slots[slot];
    return is_support(a) && a.targets_self && f.health >= kSupportBelow * f.max_health;
}
bool steady(const State& s, bool camps) {
    if (s.d > s.us.design->attack_range || s.d > s.en.design->attack_range) return false;
    if (camps) return false;
    for (int i = 0; i < 2; ++i) {
        if (s.us.cooldown[i] == 0 && usable_by_us(s.us.design->slots[i]) && !support_waiting(s.us, i)) return false;
        if (s.en.cooldown[i] == 0 && usable_by_enemy(s.en.design->slots[i])) return false;
    }
    return true;
}

// Own turns until the first ability this side would use comes off cooldown.
int turns_until_ready(const Fighter& f, bool ours) {
    int best = 1 << 30;
    for (int i = 0; i < 2; ++i) {
        const auto& a = f.design->slots[i];
        if (ours ? usable_by_us(a) : usable_by_enemy(a)) {
            if (f.cooldown[i] == 0) continue;   // only a waiting support can be ready here
            best = std::min(best, f.cooldown[i]);
        }
    }
    return best;
}
// Health below which our waiting self-support would fire (0 if none is waiting).
float support_trigger(const Fighter& f) {
    for (int i = 0; i < 2; ++i)
        if (f.cooldown[i] == 0 && support_waiting(f, i)) return kSupportBelow * f.max_health;
    return 0;
}

// True if `a` is at least as bad for us as `b`.
bool worse_for_us(const FightResult& a, const FightResult& b) {
    if (a.our_health_worst_case != b.our_health_worst_case) return a.our_health_worst_case < b.our_health_worst_case;
    if (a.we_win != b.we_win) return !a.we_win;
    return a.enemy_health_at_end > b.enemy_health_at_end;
}

struct Search {
    bool enemy_acted;
    bool same_position;
    bool our_turn_now;
    int total_turns;
    const DuelSetup& setup;
    const FightOptions& options;
    int max_leaves;
    int leaves = 0;
    std::uint64_t fuel_start = 0;
    bool over_budget = false;   // stop entirely
    int checks = 0;

    // Fuel used so far against the caller's budget, read every few turns (each read is a host call).
    void check_fuel() {
        if (!options.fuel_left || options.fuel_budget == 0 || (++checks & 3) != 0) return;
        const std::uint64_t used = fuel_start - options.fuel_left();
        if (used * 10 >= options.fuel_budget * 6) max_leaves = 0;   // no more branching
        if (used >= options.fuel_budget) over_budget = true;
    }

    FightResult finish(const State& s, int turns, bool resolved) const {
        FightResult r;
        r.resolved = resolved;
        r.we_win = resolved && dead(s.en) && !dead(s.us);
        r.our_health_worst_case = std::max(0.0f, s.us.health);
        r.enemy_health_at_end = s.en.health;
        r.turns = turns;
        return r;
    }

    // Applies everything that happens at the start of the actor's turn. Returns false if the
    // actor is stunned and loses the turn.
    bool start_turn(State& s, bool ours) const {
        Fighter& f = ours ? s.us : s.en;
        expire_shield(f);
        if (options.include_camps) {
            const float camp = ours ? setup.camp_damage_to_us_per_round : setup.camp_damage_to_enemy_per_round;
            if (camp > 0) take(f, camp);
        }
        if (f.stunned_rounds > 0) { --f.stunned_rounds; return false; }
        return !dead(f);
    }

    // Steady stretch: only basic attacks (and stunned turns, shields running down) until a
    // cooldown comes back, our self-support would fire, or someone dies. Same rules as a full
    // turn. Returns the index of the last turn played.
    int fast_forward(State& s, int t) const {
        int us_left = turns_until_ready(s.us, true), en_left = turns_until_ready(s.en, false);
        const float us_support_at = support_trigger(s.us);
        int us_turns = 0, en_turns = 0;
        for (;; ++t) {
            const bool ours = our_turn_at(enemy_acted, t, same_position, our_turn_now);
            Fighter& a = ours ? s.us : s.en;
            Fighter& o = ours ? s.en : s.us;
            expire_shield(a);
            if (a.stunned_rounds > 0) --a.stunned_rounds;
            else hit(o, a, a.strength);
            if (a.shield_turns > 0) --a.shield_turns;
            int& left = ours ? us_left : en_left;
            ++(ours ? us_turns : en_turns);
            if (dead(s.us) || dead(s.en) || --left == 0 || t + 1 >= total_turns) break;
            if (s.us.health < us_support_at) break;   // our support fires on our next turn
        }
        for (int& c : s.us.cooldown) c = std::max(0, c - us_turns);
        for (int& c : s.en.cooldown) c = std::max(0, c - en_turns);
        return t;
    }

    FightResult run(State s, int t) {
        for (; t < total_turns; ++t) {
            if (dead(s.us) || dead(s.en)) return finish(s, t, true);
            check_fuel();
            if (over_budget) { ++leaves; return finish(s, t, false); }
            const bool ours = our_turn_at(enemy_acted, t, same_position, our_turn_now);
            if (!options.on_turn && steady(s, options.include_camps)) {
                t = fast_forward(s, t);
                if (dead(s.us) || dead(s.en)) return finish(s, t + 1, true);
                continue;
            }
            if (start_turn(s, ours)) {
                if (ours) our_turn(s);
                else {
                    auto choice = choose(s);
                    if (choice.has_hold && choice.has_stun && leaves < max_leaves) {
                        // Stun timing is the one enemy choice that is searched: stun now or hold it.
                        FightResult worst;
                        bool have = false;
                        for (State* next : {&choice.stun, &choice.hold}) {
                            if (!dead(next->us) && !dead(next->en)) end_turn(next->en);
                            const auto r = run(*next, t + 1);
                            if (!have || worse_for_us(r, worst)) { worst = r; have = true; }
                        }
                        return worst;
                    }
                    s = choice.has_stun ? choice.stun : choice.hold;
                }
            }
            if (options.on_turn) options.on_turn(t, ours, s.us, s.en, s.d);
            if (dead(s.us) || dead(s.en)) return finish(s, t + 1, true);
            end_turn(ours ? s.us : s.en);
        }
        ++leaves;
        return finish(s, total_turns, false);
    }

    // Greedy enemy: best stun plan and best non-stun plan for this turn, each ranked by damage
    // to us, then distance left, then shield gained. Damage spells, resets and dashes are never
    // worse used now, so only the stun/no-stun split is left to the search.
    struct Choice { bool has_stun = false, has_hold = false; State stun, hold; Plan stun_plan{}, hold_plan{}; };
    static Choice choose(const State& s) {
        Choice c;
        const auto plans = enemy_plans(s);
        if (plans.count == 1) { c.hold = s; enemy_turn(c.hold, plans.items[0]); c.has_hold = true; c.hold_plan = plans.items[0]; return c; }
        float best_key[2] = {-1e30f, -1e30f};
        for (int k = 0; k < plans.count; ++k) {
            State next = s;
            enemy_turn(next, plans.items[k]);
            const float dmg = (s.us.health + s.us.shield) - (next.us.health + next.us.shield);
            const float key = dmg * 1000.0f - static_cast<float>(next.d) + 0.001f * (next.en.shield - s.en.shield);
            const bool stuns = next.us.stunned_rounds > s.us.stunned_rounds;
            if (key > best_key[stuns]) {
                best_key[stuns] = key;
                (stuns ? c.stun : c.hold) = next;
                (stuns ? c.has_stun : c.has_hold) = true;
                (stuns ? c.stun_plan : c.hold_plan) = plans.items[k];
            }
        }
        return c;
    }
};
}  // namespace

FightResult simulate_duel_budget(const DuelSetup& setup, const FightOptions& options, int max_leaves) {
    State s{setup.us, setup.enemy, std::max(1, setup.walk_distance)};
    Search search{setup.enemy_acted_this_round, setup.same_turn_position, setup.our_turn_now, 2 * options.round_cap, setup, options, max_leaves};
    if (options.fuel_left) search.fuel_start = options.fuel_left();
    auto r = search.run(s, 0);
    // A budget cut leaves branches unexplored: the answer is then not a proven worst case.
    if (search.over_budget) r.resolved = false;
    if (!r.resolved) r.we_win = false;
    return r;
}
FightResult simulate_duel(const DuelSetup& setup, const FightOptions& options) {
    return simulate_duel_budget(setup, options, kMaxSearchLeaves);
}
FightResult simulate_duel_fast(const DuelSetup& setup, const FightOptions& options) {
    return simulate_duel_budget(setup, options, 0);
}

Plan choose_enemy_plan(const DuelSetup& setup) {
    const State s{setup.us, setup.enemy, std::max(1, setup.walk_distance)};
    const auto c = Search::choose(s);
    return c.has_stun ? c.stun_plan : c.hold_plan;
}
