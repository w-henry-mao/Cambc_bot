#include "team_fight.h"
#include <algorithm>
#include "our_policy.h"

// Event-driven version of the team-fight estimate (team_fight.h). Instead of playing every
// round it steps from event to event (a champion arriving in reach, or a champion dying), with
// each side dealing a steady damage rate in between:
// - rate = Strength (x2 per reset cooldown) + every damage spell spread over its cooldown;
//   spells and resets ready on arrival land at once as a burst;
// - stuns cut the stunned target's rate by duration / cooldown;
// - heals and self-shields cut the damage their side's target takes by amount / cooldown;
// - Ren's retaliation is added to the Ren side's rate while Ren is targeted;
// - a champion arrives once it has walked to within reach of the nearest opponent (both sides
//   close in, so the gap shrinks twice as fast when both need to walk; dashes count once).
// Targets as in the round version: ours the least health, theirs the fastest kill, among the
// opponents that have arrived. Much cheaper (a few dozen steps at most), but spell and stun
// timing is averaged out.
namespace {
constexpr float kRetaliationFocusRatio = 0.2f;   // Ren, docs/combat-rules.md #6b
constexpr int kCassiType = 3, kIssacType = 4;
constexpr int N = kTeamFightMaxSide;

struct Q {
    float health;        // raw damage still needed to kill (health + shield, before reduction)
    float max_health;    // same, at full health
    float start_health;  // `health` when loaded (runs reset to it)
    float rate, burst;   // raw damage per round / on arrival (unscaled)
    float stun_rate;     // fraction of its target's rounds it stuns away
    float support_rate;  // heal (side) per round
    float self_shield;   // own shield per round, when targeted
    float retaliation;   // per hit taken (Ren)
    float arrive;        // round it starts fighting
    float leave_health;  // starts leaving at or below this (0: never)
    float gone_at;       // leaving: when it is out of reach
    int linked;
    bool alive, start_alive, arrived, stun_immune, burst_pending;
    bool leaving, dead;  // dead: killed (alive is false for both dead and gone)
};
struct QSide {
    Q u[N];
    int n;
    float mult;
};

void load_side(QSide& s, const TeamMember* m, int n, const int (*dist)[N], bool ours, const TeamMember* other, int other_n,
               float leave_below, int join_distance, const TeamFightOptions& o) {
    s.n = std::min(n, N);
    s.mult = 1;
    for (int i = 0; i < s.n; ++i) {
        const auto& f = m[i].f;
        auto& q = s.u[i];
        q.alive = f.design && f.health > 0;
        q.linked = -1;
        q.arrived = false;
        q.burst_pending = true;
        q.leaving = q.dead = false;
        q.leave_health = 0;
        q.gone_at = 0;
        q.health = q.max_health = q.start_health = 0;
        q.start_alive = q.alive;
        if (!q.alive) continue;
        const auto& d = *f.design;
        const float taken = std::max(0.01f, f.damage_taken_mult);
        q.health = q.start_health = (f.health + f.shield) / taken;
        q.max_health = (f.max_health + f.shield) / taken;
        q.leave_health = leave_below * f.max_health / taken;
        q.rate = f.strength;
        q.burst = 0;
        q.stun_rate = 0;
        q.support_rate = 0;
        q.self_shield = 0;
        q.stun_immune = d.passive == Passive::StunImmunity;
        q.retaliation = d.passive == Passive::Retaliation ? kRetaliationFocusRatio * f.focus : 0;
        int dash = 0;
        for (int k = 0; k < 2; ++k) {
            const auto& ab = d.slots[k];
            const float cd = static_cast<float>(std::max(1, ab.cooldown));
            const bool ready = f.cooldown[k] == 0;
            switch (ab.kind) {
                case AbilityKind::Damage: case AbilityKind::AoeDamage: case AbilityKind::Cc:
                    if (!ab.targets_enemies) break;
                    q.rate += o.spell_weight * ab.modifier * f.focus / cd;
                    if (ready) q.burst += o.spell_weight * ab.modifier * f.focus;
                    if (ab.kind == AbilityKind::Cc || ab.duration > 0) q.stun_rate += o.stun_weight * static_cast<float>(ab.duration) / cd;
                    break;
                case AbilityKind::AttackReset:
                    q.rate += f.strength / cd;
                    if (ready) q.burst += f.strength;
                    if (ab.enhanced_attack) q.stun_rate += o.stun_weight / cd;
                    break;
                case AbilityKind::Heal: q.support_rate += o.support_weight * ab.modifier * f.focus / cd; break;
                case AbilityKind::Shield: if (ab.targets_self) q.self_shield += o.support_weight * ab.modifier * f.focus / cd; break;
                case AbilityKind::Dash: if (ready) dash = static_cast<int>(ab.range); break;
                default: break;
            }
        }
        // Rounds before it is in reach of the nearest opponent.
        int nearest = 1 << 20, j_near = -1;
        for (int j = 0; j < other_n; ++j) {
            const int dj = ours ? dist[i][j] : dist[j][i];
            if (dj < nearest) { nearest = dj; j_near = j; }
        }
        if (join_distance > 0 && nearest > join_distance) {   // stays out: not in the fight, not dead
            q.alive = q.start_alive = false;
            continue;
        }
        const int my_reach = reach(d);
        int gap = std::max(0, nearest - my_reach - dash);
        if (gap > 0 && j_near >= 0 && other[j_near].f.design && reach(*other[j_near].f.design) < nearest) gap = (gap + 1) / 2;
        const int moves = std::max(1, d.move_speed);
        int rounds = (gap + moves - 1) / moves;
        if (rounds > 0 && d.ranged) ++rounds;   // ranged: walking uses the attack
        q.arrive = static_cast<float>(rounds);
    }
    for (int i = 0; i < s.n; ++i)
        for (int j = 0; j < s.n; ++j) {
            if (i == j || !m[i].f.design || !m[j].f.design) continue;
            const int a = m[i].f.design->type, b = m[j].f.design->type;
            if ((a == kCassiType && b == kIssacType) || (a == kIssacType && b == kCassiType)) s.u[i].linked = j;
        }
}

// One pass over a side per step (each pass over the units costs fuel in wasm, so everything a
// step needs is gathered at once).
struct Scan {
    int alive = 0, fighters = 0;
    int target = -1;            // its arrived champion with the least effective health: what the
                                // other side hits (ours: least health; theirs: fastest kill)
    float stun = 0, support = 0, rate = 0;
    float next_arrival = 1e9f;  // earliest arrival still to come
    float next_gone = 1e9f;     // earliest leaving champion getting out of reach
    float pending_burst = 0;    // bursts of arrived champions not yet spent
};
Scan scan(const QSide& s) {
    Scan c;
    for (int i = 0; i < s.n; ++i) {
        const auto& q = s.u[i];
        if (!q.alive) continue;
        ++c.alive;
        if (!q.arrived) { c.next_arrival = std::min(c.next_arrival, q.arrive); continue; }
        if (c.target < 0 || q.health < s.u[c.target].health) c.target = i;   // leaving ones can still be hit
        if (q.leaving) { c.next_gone = std::min(c.next_gone, q.gone_at); continue; }
        ++c.fighters;
        c.stun += q.stun_rate;
        c.support += q.support_rate;
        c.rate += q.rate;
        if (q.burst_pending) c.pending_burst += q.burst;
    }
    return c;
}

// The target dies at no health left, taking a linked Cassi/Issac with it. True if it died.
// Below its leave threshold (and not dead) it starts leaving.
bool check_death(QSide& s, int t, float now, float leave_delay) {
    if (t < 0) return false;
    auto& q = s.u[t];
    if (q.health > 1e-3f) {
        if (!q.leaving && q.health <= q.leave_health) { q.leaving = true; q.gone_at = now + leave_delay; }
        return false;
    }
    q.alive = false;
    q.dead = true;
    if (q.linked >= 0 && s.u[q.linked].alive) { s.u[q.linked].alive = false; s.u[q.linked].dead = true; }
    return true;
}
// Leaving champions out of reach by `now` are gone (alive, out of the fight).
void departures(QSide& s, float now) {
    for (int i = 0; i < s.n; ++i)
        if (s.u[i].alive && s.u[i].leaving && s.u[i].gone_at <= now + 1e-4f) s.u[i].alive = false;
}
int dead_count(const QSide& s) {
    int c = 0;
    for (int i = 0; i < s.n; ++i) c += s.u[i].dead;
    return c;
}

void spend_bursts(QSide& s) {
    for (int i = 0; i < s.n; ++i)
        if (s.u[i].arrived) s.u[i].burst_pending = false;
}
void arrivals(QSide& s, float t) {
    for (int i = 0; i < s.n; ++i)
        if (s.u[i].alive && !s.u[i].arrived && s.u[i].arrive <= t + 1e-4f) s.u[i].arrived = true;
}

// Damage per round on the defenders' target: the attackers' rates (the attacker the defenders hit
// loses part of its rate to their stuns), minus the defenders' heals and the target's shields.
float rate_on(const QSide& att, const Scan& a, const QSide& def, const Scan& d) {
    float r = a.rate;
    if (d.target >= 0 && a.target >= 0) {
        const auto& stunned = att.u[a.target];
        if (!stunned.stun_immune) r -= stunned.rate * std::min(1.0f, d.stun);
    }
    r *= att.mult;
    const float relief = def.u[d.target].self_shield + d.support;
    return std::max(0.1f * r, r - relief);
}

struct QFight { QSide us, en; };

// Back to the loaded state for another run (cheaper than loading again).
void reset(QSide& s) {
    for (int i = 0; i < s.n; ++i) {
        auto& q = s.u[i];
        q.health = q.start_health;
        q.alive = q.start_alive;
        q.arrived = false;
        q.burst_pending = true;
        q.leaving = q.dead = false;
    }
}

void load(QFight& f, const TeamFightSetup& setup, const TeamFightOptions& o) {
    load_side(f.us, setup.us, setup.us_count, setup.distance, true, setup.enemy, setup.enemy_count, o.our_leave_below, o.join_distance, o);
    load_side(f.en, setup.enemy, setup.enemy_count, setup.distance, false, setup.us, setup.us_count, o.enemy_leave_below, o.join_distance, o);
}

float health_left(const QSide& s) {
    float h = 0, m = 0;
    for (int i = 0; i < s.n; ++i) {
        m += s.u[i].max_health;
        if (s.u[i].alive) h += std::max(0.0f, s.u[i].health);
    }
    return m > 0 ? h / m : 0;
}

TeamOutcome run(QFight& f, float our_mult, float enemy_mult, int round_cap, float leave_delay) {
    f.us.mult = our_mult;
    f.en.mult = enemy_mult;
    const float cap = static_cast<float>(round_cap);
    float t = 0;
    arrivals(f.us, t);
    arrivals(f.en, t);
    Scan su, se;
    int steps = 0;
    for (; steps < 64; ++steps) {
        su = scan(f.us);
        se = scan(f.en);
        if (su.alive == 0 || se.alive == 0) break;
        if (su.fighters == 0 && se.fighters == 0 && su.next_arrival > 1e8f && se.next_arrival > 1e8f) break;   // nobody left fighting
        // su.target is what the enemy hits, se.target what we hit.
        // Bursts land on the current target as soon as there is one.
        if ((su.pending_burst > 0 && se.target >= 0) || (se.pending_burst > 0 && su.target >= 0)) {
            if (su.pending_burst > 0 && se.target >= 0) { f.en.u[se.target].health -= su.pending_burst * f.us.mult; spend_bursts(f.us); }
            if (se.pending_burst > 0 && su.target >= 0) { f.us.u[su.target].health -= se.pending_burst * f.en.mult; spend_bursts(f.en); }
            check_death(f.en, se.target, t, leave_delay);
            check_death(f.us, su.target, t, leave_delay);
            continue;
        }
        if (t >= cap) break;
        float r_en = se.target >= 0 && su.fighters > 0 ? rate_on(f.us, su, f.en, se) : 0;   // on their target
        float r_us = su.target >= 0 && se.fighters > 0 ? rate_on(f.en, se, f.us, su) : 0;   // on ours
        // Ren's retaliation: every hit on Ren costs its attacker; counted on the attackers' target.
        if (se.target >= 0 && f.en.u[se.target].retaliation > 0 && su.target >= 0)
            r_us += f.en.u[se.target].retaliation * f.en.mult * static_cast<float>(su.fighters);
        if (su.target >= 0 && f.us.u[su.target].retaliation > 0 && se.target >= 0)
            r_en += f.us.u[su.target].retaliation * f.us.mult * static_cast<float>(se.fighters);
        float dt = std::min(std::min(cap, std::min(su.next_arrival, se.next_arrival)), std::min(su.next_gone, se.next_gone)) - t;
        // Next death, or the target dropping to its leave threshold.
        auto until = [](const Q& q, float r) {
            const float to_leave = q.leaving || q.leave_health <= 0 ? 0 : q.health - q.leave_health;
            return (to_leave > 0 ? to_leave : q.health) / r;
        };
        if (r_en > 0) dt = std::min(dt, until(f.en.u[se.target], r_en));
        if (r_us > 0) dt = std::min(dt, until(f.us.u[su.target], r_us));
        if (r_en <= 0 && r_us <= 0 && su.next_arrival > 1e8f && se.next_arrival > 1e8f) { t = cap; break; }   // nobody will ever meet
        dt = std::max(dt, 0.0f);
        if (r_en > 0) {
            auto& h = f.en.u[se.target].health;
            h -= r_en * dt;
            if (h < 1e-2f * r_en) h = 0;   // float leftovers from the division count as a kill
        }
        if (r_us > 0) {
            auto& h = f.us.u[su.target].health;
            h -= r_us * dt;
            if (h < 1e-2f * r_us) h = 0;
        }
        t += dt;
        // Threshold crossings land within float error of the leave health: snap them.
        for (auto* p : {&f.en.u[se.target < 0 ? 0 : se.target], &f.us.u[su.target < 0 ? 0 : su.target]})
            if (p->leave_health > 0 && !p->leaving && p->health > 0 && p->health - p->leave_health < 1e-2f) p->health = p->leave_health;
        check_death(f.en, se.target, t, leave_delay);
        check_death(f.us, su.target, t, leave_delay);
        if (t + 1e-4f >= su.next_arrival) arrivals(f.us, t);
        if (t + 1e-4f >= se.next_arrival) arrivals(f.en, t);
        if (t + 1e-4f >= su.next_gone) departures(f.us, t);
        if (t + 1e-4f >= se.next_gone) departures(f.en, t);
    }
    TeamOutcome out;
    out.steps = steps;
    out.rounds = static_cast<int>(t + 0.999f);
    su = scan(f.us);
    se = scan(f.en);
    out.our_alive = su.alive;
    out.enemy_alive = se.alive;
    out.our_dead = dead_count(f.us);
    out.enemy_dead = dead_count(f.en);
    // Win: the other side has nobody left in the fight while some of ours still fight.
    out.we_win = out.enemy_alive == 0 && su.fighters > 0;
    out.they_win = out.our_alive == 0 && se.fighters > 0;
    out.our_health_left = health_left(f.us);
    out.enemy_health_left = health_left(f.en);
    return out;
}
}  // namespace

TeamOutcome simulate_team_fight_quick(const TeamFightSetup& setup, float our_damage_mult, float enemy_damage_mult, int round_cap,
                                      const TeamFightOptions& options) {
    QFight f;
    load(f, setup, options);
    return run(f, our_damage_mult, enemy_damage_mult, round_cap, options.leave_delay);
}

TeamEstimate estimate_team_fight_quick(const TeamFightSetup& setup, float margin, int round_cap, const TeamFightOptions& options) {
    TeamEstimate e;
    e.focus_target = first_focus_target(setup);
    QFight f;
    load(f, setup, options);
    e.outcome = run(f, 1, margin, round_cap, options.leave_delay);
    e.runs = 1;
    if (e.outcome.we_win) { e.call = TeamCall::Fight; return e; }
    reset(f.us);
    reset(f.en);
    e.outcome = run(f, margin, 1, round_cap, options.leave_delay);
    e.runs = 2;
    e.call = e.outcome.they_win ? TeamCall::Avoid : TeamCall::Unsure;
    return e;
}
