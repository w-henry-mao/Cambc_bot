#include "team_fight.h"
#include <algorithm>
#include "our_policy.h"

namespace {
constexpr float kRetaliationFocusRatio = 0.2f;   // Ren, docs/combat-rules.md #6b
constexpr int kCassiType = 3, kIssacType = 4;     // cambc::champion_type values
constexpr int N = kTeamFightMaxSide;

// Each ability reduced to what the estimate uses, once per run.
enum class Role : std::uint8_t { None, Offensive, Reset, Dash, Heal, Shield };
// Plain data (no default member values): filling only the used entries keeps setup cheap in wasm,
// where whole-struct copies and clears cost thousands of fuel.
struct Slot {
    Role role;
    float amount;          // damage, heal or shield points (unscaled)
    int stun;              // rounds (offensive), or 1 for an enhanced reset
    int cooldown, duration, range;
    bool self, allies;
};

struct Unit {
    float health, max_health, shield, taken_mult, strength;
    float retaliation;     // damage back per hit taken (Ren), unscaled
    float incoming;        // damage landing at the end of this round (scaled, before reduction)
    int stun_in;           // stun landing at the end of this round
    int hits, target;      // this round's hits and their target (for retaliation); target -1: none
    int shield_turns, stunned, reach, move, linked;
    int cd[2];
    Slot slot[2];
    bool alive, ranged, stun_immune, support;
};
struct Side {
    Unit u[N];
    int n;
    float dmg_mult, max_total;
};
struct Fight {
    Side us, en;
    int dist[N][N];   // [ours][theirs]
};

Slot slot_from(const AbilityDesign& ab, float focus) {
    Slot s;
    s.role = Role::None;
    s.amount = 0;
    s.stun = 0;
    s.cooldown = ab.cooldown;
    s.duration = ab.duration;
    s.self = ab.targets_self;
    s.allies = ab.targets_allies;
    s.range = static_cast<int>(ab.range);
    switch (ab.kind) {
        case AbilityKind::Damage: case AbilityKind::AoeDamage: case AbilityKind::Cc:
            if (!ab.targets_enemies) break;
            s.role = Role::Offensive;
            s.amount = ab.modifier * focus;
            s.stun = (ab.kind == AbilityKind::Cc || ab.kind == AbilityKind::AoeDamage) ? ab.duration : 0;
            break;
        case AbilityKind::AttackReset: s.role = Role::Reset; s.stun = ab.enhanced_attack ? 1 : 0; break;
        case AbilityKind::Dash: s.role = Role::Dash; break;
        case AbilityKind::Heal: s.role = Role::Heal; s.amount = ab.modifier * focus; break;
        case AbilityKind::Shield: if (ab.targets_self) { s.role = Role::Shield; s.amount = ab.modifier * focus; } break;
        default: break;
    }
    return s;
}

void load_side(Side& s, const TeamMember* m, int n) {
    s.n = std::min(n, N);
    s.dmg_mult = 1;
    s.max_total = 0;
    for (int i = 0; i < s.n; ++i) {
        const auto& f = m[i].f;
        auto& u = s.u[i];
        s.max_total += f.max_health;
        u.alive = f.design && f.health > 0;
        u.health = f.health; u.max_health = f.max_health; u.shield = f.shield; u.shield_turns = f.shield_turns;
        u.taken_mult = f.damage_taken_mult; u.strength = f.strength;
        u.stunned = f.stunned_rounds;
        u.incoming = 0; u.stun_in = 0; u.hits = 0; u.target = -1;
        u.linked = -1;
        u.support = false;
        u.retaliation = 0;
        u.cd[0] = f.cooldown[0];
        u.cd[1] = f.cooldown[1];
        if (!u.alive) continue;
        const auto& d = *f.design;
        u.reach = reach(d);
        u.move = d.move_speed;
        u.ranged = d.ranged;
        u.stun_immune = d.passive == Passive::StunImmunity;
        if (d.passive == Passive::Retaliation) u.retaliation = kRetaliationFocusRatio * f.focus;
        for (int k = 0; k < 2; ++k) {
            u.slot[k] = slot_from(d.slots[k], f.focus);
            if (u.slot[k].role == Role::Heal || u.slot[k].role == Role::Shield) u.support = true;
        }
    }
    for (int i = 0; i < s.n; ++i)
        for (int j = 0; j < s.n; ++j) {
            if (i == j || !m[i].f.design || !m[j].f.design) continue;
            const int a = m[i].f.design->type, b = m[j].f.design->type;
            if ((a == kCassiType && b == kIssacType) || (a == kIssacType && b == kCassiType)) s.u[i].linked = j;
        }
}

void load(Fight& f, const TeamFightSetup& setup) {
    load_side(f.us, setup.us, setup.us_count);
    load_side(f.en, setup.enemy, setup.enemy_count);
    for (int i = 0; i < f.us.n; ++i)
        for (int j = 0; j < f.en.n; ++j) f.dist[i][j] = setup.distance[i][j];
}

int alive_count(const Side& s) {
    int c = 0;
    for (int i = 0; i < s.n; ++i) c += s.u[i].alive;
    return c;
}
float health_left(const Side& s) {
    float h = 0;
    for (int i = 0; i < s.n; ++i)
        if (s.u[i].alive) h += s.u[i].health;
    return s.max_total > 0 ? h / s.max_total : 0;
}

// Distances from champion i of one side to each opponent: row i (ours) or column i (theirs).
struct Row { int* d; int stride; int operator[](int j) const { return d[j * stride]; } int& at(int j) { return d[j * stride]; } };
Row row(Fight& f, bool ours, int i) { return ours ? Row{&f.dist[i][0], 1} : Row{&f.dist[0][i], N}; }

// Target among opponents within `reach`; -1 if none.
// Ours: least health (attack.cpp weakest_enemy_within). Theirs: dies fastest, compared as
// (health + shield) / damage reduction without dividing.
int pick_target(const Side& other, const Row& d, int reach, bool ours) {
    int best = -1;
    for (int j = 0; j < other.n; ++j) {
        const auto& t = other.u[j];
        if (!t.alive || d[j] > reach) continue;
        if (best < 0) { best = j; continue; }
        const auto& b = other.u[best];
        const bool better = ours ? t.health < b.health
                                 : (t.health + t.shield) * b.taken_mult < (b.health + b.shield) * t.taken_mult;
        if (better) best = j;
    }
    return best;
}

void take(Unit& t, float amount) {
    amount *= t.taken_mult;
    const float absorbed = std::min(t.shield, amount);
    t.shield -= absorbed;
    t.health -= amount - absorbed;
}

// Heal or shield while below kSupportBelow (casting.cpp cast_support). True if cast.
bool support_cast(Side& own, int i) {
    auto& me = own.u[i];
    for (int k = 0; k < 2; ++k) {
        const auto& sl = me.slot[k];
        if (me.cd[k] != 0) continue;
        if (sl.role == Role::Shield && me.health < kSupportBelow * me.max_health) {
            me.shield = sl.amount;
            me.shield_turns = sl.duration;
            me.cd[k] = sl.cooldown;
            return true;
        }
        if (sl.role == Role::Heal) {
            // The ally missing the most health below the threshold.
            int best = -1;
            float best_missing = 0;
            for (int j = 0; j < own.n; ++j) {
                const auto& a = own.u[j];
                if (!a.alive || (j == i ? !sl.self : !sl.allies) || a.health >= kSupportBelow * a.max_health) continue;
                const float missing = a.max_health - a.health;
                if (missing > best_missing) { best = j; best_missing = missing; }
            }
            if (best < 0) continue;
            auto& t = own.u[best];
            t.health = std::min(t.max_health, t.health + sl.amount);
            me.cd[k] = sl.cooldown;
            return true;
        }
    }
    return false;
}

// A champion walks one step (plus a ready dash): every distance from it shrinks.
void step(Unit& u, Row& d, int other_n) {
    int s = u.move;
    for (int k = 0; k < 2; ++k)
        if (u.slot[k].role == Role::Dash && u.cd[k] == 0) { s += u.slot[k].range; u.cd[k] = u.slot[k].cooldown; break; }
    for (int j = 0; j < other_n; ++j) d.at(j) = std::max(1, d[j] - s);
}

void play_side(Fight& f, bool ours) {
    Side& own = ours ? f.us : f.en;
    Side& other = ours ? f.en : f.us;
    for (int i = 0; i < own.n; ++i) {
        auto& u = own.u[i];
        u.hits = 0;
        u.target = -1;
        if (!u.alive) continue;
        if (u.stunned > 0) { --u.stunned; continue; }
        bool cast = u.support && support_cast(own, i);
        Row d = row(f, ours, i);
        int t = pick_target(other, d, u.reach, ours);
        if (t < 0) {
            step(u, d, other.n);
            if (u.ranged) continue;   // ranged: walking uses the attack
            t = pick_target(other, d, u.reach, ours);
            if (t < 0) continue;
        }
        auto& tgt = other.u[t];
        float dmg = 0;
        int hits = 1;
        if (!cast)
            for (int k = 0; k < 2; ++k)
                if (u.slot[k].role == Role::Offensive && u.cd[k] == 0 && d[t] <= u.slot[k].range) {
                    dmg += u.slot[k].amount;
                    if (u.slot[k].amount > 0) ++hits;
                    tgt.stun_in = std::max(tgt.stun_in, u.slot[k].stun);
                    u.cd[k] = u.slot[k].cooldown;
                    cast = true;
                    break;
                }
        dmg += u.strength;
        if (!cast)
            for (int k = 0; k < 2; ++k)
                if (u.slot[k].role == Role::Reset && u.cd[k] == 0) {
                    dmg += u.strength;
                    ++hits;
                    tgt.stun_in = std::max(tgt.stun_in, u.slot[k].stun);
                    u.cd[k] = u.slot[k].cooldown;
                    break;
                }
        tgt.incoming += dmg * own.dmg_mult;
        u.hits = hits;
        u.target = t;
    }
}

// The round's damage lands; Ren's retaliation goes back to each attacker per hit.
void land(Side& attackers, Side& defenders) {
    for (int i = 0; i < attackers.n; ++i) {
        auto& a = attackers.u[i];
        if (a.target >= 0 && defenders.u[a.target].retaliation > 0)
            take(a, static_cast<float>(a.hits) * defenders.u[a.target].retaliation * defenders.dmg_mult);
    }
    for (int j = 0; j < defenders.n; ++j) {
        auto& t = defenders.u[j];
        if (t.incoming > 0) take(t, t.incoming);
        if (t.stun_in > 0 && !t.stun_immune) t.stunned = std::max(t.stunned, t.stun_in);
        t.incoming = 0;
        t.stun_in = 0;
    }
}

// Deaths, then timers. Returns how many are still alive.
int end_round(Side& s) {
    for (int i = 0; i < s.n; ++i)
        if (s.u[i].alive && s.u[i].health <= 0) {
            s.u[i].alive = false;
            if (s.u[i].linked >= 0) s.u[s.u[i].linked].alive = false;   // Cassi and Issac die together
        }
    int alive = 0;
    for (int i = 0; i < s.n; ++i) {
        auto& u = s.u[i];
        if (!u.alive) continue;
        ++alive;
        if (u.cd[0] > 0) --u.cd[0];
        if (u.cd[1] > 0) --u.cd[1];
        if (u.shield_turns > 0) --u.shield_turns;
        else u.shield = 0;
    }
    return alive;
}

TeamOutcome run(Fight& f, float our_damage_mult, float enemy_damage_mult, int round_cap) {
    f.us.dmg_mult = our_damage_mult;
    f.en.dmg_mult = enemy_damage_mult;
    TeamOutcome out;
    int r = 0;
    int ours = alive_count(f.us), theirs = alive_count(f.en);
    for (; r < round_cap && ours > 0 && theirs > 0; ++r) {
        play_side(f, true);
        play_side(f, false);
        land(f.us, f.en);   // our hits on them (and retaliation onto us)
        land(f.en, f.us);
        ours = end_round(f.us);
        theirs = end_round(f.en);
    }
    out.rounds = r;
    out.our_alive = ours;
    out.enemy_alive = theirs;
    out.we_win = theirs == 0 && ours > 0;
    out.they_win = ours == 0 && theirs > 0;
    out.our_health_left = health_left(f.us);
    out.enemy_health_left = health_left(f.en);
    return out;
}
}  // namespace

int first_focus_target(const TeamFightSetup& setup) {
    int best = -1;
    for (int i = 0; i < setup.enemy_count && i < N; ++i) {
        const auto& f = setup.enemy[i].f;
        if (f.health > 0 && f.design && (best < 0 || f.health < setup.enemy[best].f.health)) best = i;
    }
    return best;
}

TeamOutcome simulate_team_fight(const TeamFightSetup& setup, float our_damage_mult, float enemy_damage_mult, int round_cap) {
    Fight f;
    load(f, setup);
    return run(f, our_damage_mult, enemy_damage_mult, round_cap);
}

TeamEstimate estimate_team_fight(const TeamFightSetup& setup, float margin, int round_cap) {
    TeamEstimate e;
    e.focus_target = first_focus_target(setup);
    Fight f;
    load(f, setup);
    e.outcome = run(f, 1, margin, round_cap);
    e.runs = 1;
    if (e.outcome.we_win) { e.call = TeamCall::Fight; return e; }
    load(f, setup);   // reload (only the entries in use) rather than keep a copy
    e.outcome = run(f, margin, 1, round_cap);
    e.runs = 2;
    e.call = e.outcome.they_win ? TeamCall::Avoid : TeamCall::Unsure;
    return e;
}
