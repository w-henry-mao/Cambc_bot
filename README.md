# CAMBC Bot Skeleton

A modular skeleton for CAMBC C++ bots, organized for parallel development without merge conflicts.

## Directory Layout

```
src/
├── main.cpp                     Dispatch only (draft vs gameplay)
├── bot.h / bot.cpp              Bot: turn guard -> lazy ctx -> selector -> post_turn()
│
├── init/                        Staged initialization (runs once on first play turn)
│   ├── initializer.h/.cpp       Stage runner
│   ├── stages.h                 Declarations of the stage functions below
│   ├── map_snapshot.cpp         Cache static map data
│   ├── roster.cpp               Allied champions
│   ├── self_setup.cpp           Own champion type -> profile
│   └── draft_memory_read.cpp    Decode draft memory
│
├── core/
│   ├── match_data.h             Static facts from init (read-only)
│   ├── context.h/.cpp           Per-turn TurnContext (lazy)
│   ├── bot_memory.h             Persistent mutable data
│   ├── geometry.h               Distance, tiles_around
│   ├── map_snapshot_shared.h/.cpp   Shared by draft and init
│   └── rng.h                   Seeded RNG (WASI randomness is zero)
│
├── constants/                   Decision constants (not game facts)
│   ├── brain_constants.h        State scores, switch margins, fuel reserves
│   ├── state_constants.h        Recall HP fraction, default shard stat
│   ├── nav_constants.h          Minimum fuel for pathfinding
│   └── draft_constants.h
│
├── comms/
│   ├── draft_memory.h           1,024-byte layout (versioned header)
│   └── shared_memory.h          8-byte layout helpers
│
├── draft/draft.h/.cpp           Placeholder random pick
│
├── nav/
│   ├── blocked_tiles.h/.cpp      Compute blocked tiles
│   ├── safe_recall.h/.cpp       Where to recall from: out of reach of visible enemies and awake camps
│   ├── safe_spot_score.h        Tiebreak score between safe tiles (no SDK; host-tested)
│   ├── pathfinding.h/.cpp       Distance field pathing
│   └── movement.h/.cpp          Movement utilities
│
├── brain/
│   ├── state.h                  State enum (Recall, Attack, Tribute, Farm)
│   └── state_selector.h/.cpp    Commitment + switch guard (hysteresis)
│
├── states/
│   ├── state_module.h           State interface (score, execute, on_enter, on_exit)
│   ├── shop.h                   Pre-step: buy items on spawn/dead
│   ├── recall.h/.cpp            Recall when hurt away from spawn
│   ├── attack.h/.cpp            Attack enemy champions in range
│   ├── tribute.h/.cpp           Submit gems at altars
│   └── farm.h/.cpp              Attack camps or move toward spawn tiles
│
├── skills/                      Skill stubs (empty + TODO)
│   ├── skill.h                  Skill interface (can_use, use)
│   ├── melee_attack.h/.cpp      Basic attack for melee
│   ├── ranged_attack.h/.cpp     Basic attack for ranged
│   ├── dash, attack_reset, single_target_damage, aoe_damage, stun,
│       shield, heal, ward, consumables  (.h/.cpp each)
│   └── skill_instances.h       Includes every skill header (class + instance)
│
├── champions/
│   ├── champion_profile.h       Attack type, skill list, linked_pair flag
│   ├── registry.cpp/.h          champion_type -> profile
│   └── ryth, stellar, tavi, willow, cassi, issac, prism, vesper_vale,
│       vendetta, marcella, ren, varg  (.cpp each, stub profiles)
│
├── combat/                      Fight calculator (docs/combat-accuracy.md, docs/combat-rules.md)
│   ├── model.h, design_table.h  Pure model types; generated champion data (tools/gen_design_table.py)
│   ├── simulator.h/.cpp         Duel simulation: our attack logic vs a max-aggression enemy
│   ├── our_policy.h/.cpp        Combat decisions shared with states/attack.cpp, skills/casting.cpp
│   ├── turn_order.h/.cpp        Who acts when
│   ├── sdk_adapter.h/.cpp       predict_duel_fast/full, two_v_one_favourable (live SDK state in)
│   ├── team_fight.h/.cpp        Rough team-fight estimate, round by round
│   ├── team_fight_quick.cpp     Same estimate, event-driven (cheaper, rougher)
│   ├── sdk_team_fight.h/.cpp    estimate_team_fight_here, fight calls in shared memory
│   └── sdk_trace, sdk_duel_mode Test-build logging and duel mode (off in normal builds)
│
└── debug/
    ├── log.h                    constexpr bool kLogging (compiles to nothing when off)
    ├── fuel_meter.h             Scoped fuel-used helper
    └── overlay.h                Debug drawing wrappers
```

## Per-Turn Flow

1. **TURN GUARD** (in `bot.cpp`):
   - Dead champions: shop only, reset state, return
   - Alive but recalling/submitting/teleporting: return
   - Cannot act (no move or attack): return

2. **Shop pre-step** (in `states/shop.h`):
   - Buy power shards on spawn or when dead
   - Never consumes the turn

3. **Build lazy TurnContext** (in `core/context.h`):
   - Cached per-turn data: enemies, camps, resources, stats
   - Includes champion profile
   - Stamped with round_number for suspension safety

4. **SELECTOR picks state** (in `brain/state_selector.cpp`):
   - Score all states (cheap operation)
   - Use hysteresis: challenger must beat current by margin
   - Call on_exit on switch, on_enter for new state
   - On failure, try next-highest state once

   Just before scoring, `decide_team_fight_call()` (`brain/team_fight_call.cpp`) may call a team
   fight. It is empty for now: the team decides when to call.

5. **Execute state** (in `states/*.cpp`):
   - Wrapped in try/catch for `cambc::error`
   - Return StateResult: Continue, Done, or Failed

6. **Post-turn hook** (empty in `bot.cpp`)

## State Machine

**Commitment with hysteresis**: The bot keeps executing a state until it's done or another state clearly beats it.

```cpp
enum class StateResult { Continue, Done, Failed };

struct StateModule {
    virtual int score(const TurnContext&) = 0;      // <= 0 means not applicable
    virtual StateResult execute(TurnContext&, BotMemory&) = 0;
    virtual void on_enter(TurnContext&, BotMemory&) {}
    virtual void on_exit(TurnContext&, BotMemory&) {}
    virtual int switch_margin() const { return 10; }
};
```

**Selector hysteresis (switch guard)**:
- Current state persists unless:
  - Returns Done/Failed
  - Challenger score > current score + switch_margin
- Prevents thrashing between closely-scored states

**Scores** (from `constants/brain_constants.h`):
- Recall: 100
- Attack: 80
- Tribute: 60
- Farm: 40
- Default switch margin: 10 (must stay below score gaps)

**Shop** is a pre-step, not a competing state.

## Staged Init

Staged initialization runs on the first play turn, resumable if fuel runs out:

1. `map_snapshot`: Cache camp groups and static blockers
2. `roster`: Cache allied champions
3. `self_setup`: Resolve own champion profile
4. `draft_memory_read`: Decode draft memory

Each stage checks `fuel_remaining()` against `kInitFuelReserve` before proceeding.

Init stages run before baseline states, so states must not depend on `MatchData` being complete.

## Draft Specifics

- `champion_draft` (11 values): Cassi & Issac are ONE pick
- `champion_type` (12 values): Cassi and Issac are separate
- Registry keyed by `champion_type`
- Use `draft::team_size()` instead of hardcoding
- `self()` throws during draft
- `core/map_snapshot_shared` must guard with `is_draft()` before calling `self()`

## How to Add a State

1. Create `states/my_state.h` declaring a class that implements `StateModule` and `extern MyState my_state_instance;`
2. Create `states/my_state.cpp` defining `score()`, `execute()` (and optionally `on_enter()`, `on_exit()`, `switch_margin()`) and the instance
3. Add a memory sub-struct in `core/bot_memory.h`
4. Include `states/my_state.h` in `bot.cpp` (never include a `.cpp`: every `.cpp` under `src/` is already compiled) and add the state to the selector:
   ```cpp
   selector.add_state(&my_state_instance, State::MyState);
   ```
5. Add State enum to `brain/state.h` if needed
6. Add score constant to `constants/brain_constants.h`

## How to Add a Champion

1. Create `champions/my_champ.cpp` with:
   ```cpp
   #include "champion_profile.h"
   #include "../skills/skill_instances.h"
   ChampionProfile my_champ_profile{.is_ranged = false, .skills = {&skill1_instance, &skill2_instance}, .linked_pair = false};
   ```
2. Add entry to registry in `champions/registry.cpp` (one line):
   ```cpp
   {cambc::champion_type::my_champ, &my_champ_profile},
   ```

## How to Add a Skill

1. Create `skills/my_skill.h` declaring the class (deriving from `Skill`) and `extern MySkill my_skill_instance;`
2. Create `skills/my_skill.cpp` defining `can_use()`, `use()` and the instance
3. Include the header in `skills/skill_instances.h`
4. Add instance to champion profiles in `champions/*.cpp`

## Safe Recall

Damage cancels a recall, so the recall state (`states/recall.cpp`) first gets out of reach of
everything it can see (`nav/safe_recall.h`):
- Danger: tiles within reach of a visible enemy champion (its longest attack/damage/stun range,
  from the design table, plus `kRecallEnemyBuffer` = 2) or of an awake camp (range 3 plus
  `kRecallCampBuffer` = 0). A camp counts as awake when it or a camp within 3 tiles is damaged.
- Safe where we stand and not hit since last turn: recall. Otherwise walk to the best safe tile
  within `kRecallSearchSteps` (8) steps and recall on arrival (moving and recalling in one turn
  works). Hit by something we cannot see, or no safe tile in range: walk home as before.
- Best safe tile: highest weighted score (`kRecallWeight*`, `constants/state_constants.h`): fewer
  steps, shorter walk home, closer to the nearest visible teammate, farther from the other
  visible enemies (all but the closest), plus `kRecallBushBonus` for a bush (hidden from
  champions outside it; camps see through bushes). Only tiles within `kRecallExtraSteps` (2) of
  the nearest safe tile are scored, plain tiles on a 2x2 grid plus every bush tile.
- Fuel: median 15.8k per retreat turn, 99% under 31k, worst seen 52k (13,377 searches).
- `kSafeRecall` = false restores the old behaviour.

Tested on 204 matches (6 opponents x 17 maps), the same bot with and without it, then a sweep of
the constants (306 more):

| build | retreats | reached home | died on the way | recalls cancelled per retreat |
| --- | --- | --- | --- | --- |
| old recall | 3111 | 67.0% | 32.8% | 0.46 |
| safe recall (buffer 2, bush bonus 3) | 3223 | 73.9% | 26.0% | 0.03 |
| buffer 1 | 3340 | 73.0% | 26.8% | 0.05 |
| buffer 3 | 3228 | 74.4% | 25.3% | 0.03 |
| no bush bonus | 3287 | 72.9% | 27.0% | 0.03 |

Against the most aggressive opponents (opp-rush, opp-caller) deaths on the way home fell from
46-47% to 35%. Logging: `RECALL_TRACE=true` test builds, `python3 tools/recall_report.py <dir>`;
sweep switches `SAFE_RECALL`, `RECALL_ENEMY_BUFFER`, `RECALL_BUSH_BONUS`.

## Fight Calculator

```cpp
#include "combat/sdk_adapter.h"
FightResult r = predict_duel_fast(enemy);   // normal turns: <=15k fuel of simulation, may be unresolved
FightResult r = predict_duel_full(enemy);   // spare-fuel turns (recall/tribute): searches stun timing
bool ok = two_v_one_favourable(ally_a, ally_b, enemy);
```

`FightResult` gives `we_win`, `our_health_worst_case` (enemy maximally aggressive),
`enemy_health_at_end`, `turns` and `resolved`. Call with a visible enemy only (otherwise the
result is unresolved).

Things callers should know:
- The model has our champion keep fighting the way the attack state does. In real games the
  recall state (score 100) takes over below 35% health, so a real fight may end in a retreat
  rather than the predicted finish.
- `resolved == false` means "no answer": fuel ran short, the round cap was reached, or the
  enemy is not visible. Treat it as unknown, not as a loss.
- The fast version spends at most 15,000 fuel simulating, never more than the turn has left
  after a 25,000 reserve; the full version uses everything above that reserve.
- `two_v_one_favourable` never takes the "either ally wins alone" shortcut for Cassi and Issac,
  who die together.

**Path cache (fast version).** Each fast prediction stores the predicted state at the start of
our next turns (`kPathCachePoints` = 8). If on a later turn the fight has gone exactly as
predicted, the stored answer is reused without simulating (`combat/path_cache.h`). Only for our
own champion, without camp damage, and only paths for the same matchup are checked. Measured vs
starter-cpp: a hit costs ~6,400 fuel, a miss ~19,900, no cache ~18,300; it pays off above
roughly a 12% hit rate. Hit rates seen: 90% vs a maximally aggressive opponent, 41% vs
starter-cpp, 5-14% in self-play. Switch: `kPathCacheEnabled` (constants/combat_constants.h).

## Team Fights

```cpp
#include "combat/sdk_team_fight.h"
TeamFightCall c = estimate_team_fight_here();   // c.estimate.call: Fight / Avoid / Unsure
if (c.estimate.call == TeamCall::Fight && c.target) call_team_fight(*c.target);
```

The estimate takes us, the allies we can see and the enemies we can see within
`kTeamFightRadius` (8). Vision is not shared, so allies out of our own vision are left out. It
plays the fight out with each champion attacking the weakest opponent within its reach (as
`states/attack.cpp` does) and answers **Fight** if we still win with the enemy's damage x
`kTeamFightMargin` (1.3), **Avoid** if they still win with ours x 1.3, else **Unsure**. Rules
and simplifications are listed in `combat/team_fight.h`. Like the duel predictor it assumes
everyone fights to the end (no recall).

Two versions with the same interface: round by round (`estimate_team_fight`) and event-driven
(`estimate_team_fight_quick`, the default via `kTeamFightQuick`). In 20 test matches (10 maps,
self-play and vs starter-cpp; outcome = deaths among those champions over the next 20 rounds,
estimates with no deaths left out), on the 129 estimates where both ran (the round version was
skipped on turns with little fuel left):

| call | round version: we lost fewer / more | quick version: we lost fewer / more |
| --- | --- | --- |
| Fight | 79% / 15% (53) | 79% / 15% (53) |
| Unsure | 55% / 45% (20) | 55% / 44% (18) |
| Avoid | 30% / 57% (56) | 31% / 56% (58) |

Over all 357 quick estimates (busy turns included): Fight 73% / 17%, Unsure 57% / 40%, Avoid
28% / 62%. The two agreed on 167 of 183 calls. Fuel (median), reading the live state plus estimating:
3 champions ~5k + ~5k (quick) or ~13k (round); 6 champions ~10k + ~12k (quick) or ~31k
(round). The round version can exceed 50k with 6 or more champions.

**Accuracy work (tools/team_resim.cpp).** Every TEAM_PRED line logs the full model input, so
`tools/team_dataset.py` + `tools/team_resim.cpp` replay thousands of logged fights through model
variants in seconds (it first checks it reproduces the logged calls: 100% on current logs). In
two 391-match batches and a 68-match confirmation batch:
- Heals and shields at half weight (`kTeamFightSupportWeight` = 0.5) ranked real fights best and
  held up on fresh data; spells and stuns are about right as modelled.
- No gain from: modelling recall on either side, leaving distant champions out, a 20-round cap,
  or adding enemies this champion saw recently but cannot see now.
- The main error is reinforcements: with nobody arriving later the estimate ranks fights far
  better (AUC 0.85 vs 0.72 when enemies or allies join). Accuracy also falls with time after
  the estimate (AUC 0.87 over the next 5 rounds, 0.80 over 20), so a fresh estimate every few
  rounds is worth more than one held for long.

**Teammate positions (comms/ally_positions.h).** Bytes 6-7 of shared memory carry one
teammate's position (2-tile resolution), roster index and a half-health bit; teammates take
turns, one per round, so each is heard every few rounds. `share_ally_positions()` runs every turn
(~300 fuel, ~600 when writing). The estimate adds teammates it cannot see whose report is at most
`kAllyPositionMaxAge` (3) rounds old and within `kTeamFightSharedRadius` (8) tiles, with the stats
this champion last saw them with (`remember_teammates()` refreshes those every 10 rounds). Tested
on 340 matches with forced drafts covering all 12 champions: ranking improves a little (AUC
0.780 -> 0.783 tuning, 0.810 -> 0.825 held-out) with call accuracy at margin 1.3 unchanged (64%).
Wider radii or older reports rank about the same but call worse (down to 60%): a reported
teammate actually joins only ~55% of the time. Switch: `kTeamFightSharedAllies`.

**Per champion.** With forced drafts (`DRAFT_PICKS=1,2,5,6,8` etc. in test builds), every
champion appeared in over 500 estimated fights; calls over the next 10 rounds were right 73-79%
of the time with any one champion on our side and 80-84% with it on the enemy side, so no
champion is misjudged by the model.

**Calling and following (test caller, 408 matches, 6 opponents x 17 maps).** Logged calls,
non-starter opponents, outcome = who lost more champions:

| our build | calls right, next 10 rounds | next 20 rounds | Fight calls right (20) |
| --- | --- | --- | --- |
| no calls | 68% | 63% | 52% |
| estimate every turn, call on Fight, team follows | 76% | 71% | 68% |
| estimate every 3 rounds (calls last 3 rounds) | 75% | 70% | 68% |
| every 3 rounds + reported teammates within 12 tiles, reports <= 6 rounds | 78% | 74% | 70% |

A followed call makes the estimate come true more often; every 3 rounds is as good as every turn
for a third of the estimates; and once teammates follow calls, counting reported teammates
farther out pays off (without following, 8 tiles / 3 rounds was best). Test switches:
`TEAM_FIGHT_TEST_CALLER`, `TEAM_FIGHT_TEST_CALL_EVERY`, `TEAM_FIGHT_CALL_ROUNDS`,
`TEAM_FIGHT_SHARED_RADIUS`, `ALLY_POSITION_MAX_AGE`, `DRAFT_PICKS`.

**Fight calls.** `call_team_fight(target)` writes shared memory bytes 2-5 (enemy, round, last
seen position); a call lapses after `kTeamFightCallRounds` (2) rounds, so the caller repeats it
each turn while the fight is on. While a call is live, every teammate's attack state hits the
called enemy whenever it is within reach, otherwise the weakest enemy in reach as before, and
with no enemy in reach it walks to the called enemy (or where it was last seen). That walk scores
`kTeamFightCallScore` (80), so recall (100) still wins over it. With no call, play is unchanged
(identical replays checked). `TEAM_FIGHT_TEST_CALLER=true` test builds call whenever the estimate
says Fight, to exercise the follow-up. Report: `python3 tools/team_fight_report.py <replay-dir>`
on `TEAM_FIGHT_TRACE=true` builds.

Tests: `make -C tests test` (host, no SDK). Exact duels: `python3 tools/duel_harness.py`.
In-match accuracy: `python3 tools/accuracy_runs.py`. Test builds come from
`tools/make_test_variant.py` (overrides in a git-ignored `src/constants/test_overrides.h`).

