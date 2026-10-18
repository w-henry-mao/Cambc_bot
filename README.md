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
│   ├── state_selector.h/.cpp    Commitment + switch guard (hysteresis)
│   └── channel_safety.h/.cpp    Whether a recall/tribute in progress should continue
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
   - Teleporting: return (cannot be cancelled)
   - Recalling or tributing: ask `channel_still_safe()` (in `brain/channel_safety.cpp`).
     Safe: return and keep channelling. Not safe: cancel it and continue the turn normally.
     Not finished: the rule waits on the fight calculator (task 8); until then it always returns safe.
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

## Shared Memory

All 8 bytes are laid out in `comms/shared_memory.h`: bytes 0-3 are one packed word holding the
farm target (6-bit group, 3-bit round) and the fight call (3-bit enemy, 6-bit round, 7-bit x
and y); bytes 4-5 the teammate position report; bytes 6-7 are free. Rounds are stored modulo 8
and 64, so `sweep_team_header()` clears expired entries at the start of every champion's turn
(dead ones too). Repacking was checked against the previous layout: identical replays in 12
normal matches and in 24 test-caller matches with unlimited fuel; it also fixes a wrap-around
bug in the old 8-bit round stamp that let a fight call come back exactly 256 rounds later.

## Shop Trips (off; best vs v8 at 150 gold)

`SHOP_RECALL_GOLD` (`states/recall.cpp`, 0 = off): recall to shop once a champion holds that much gold, unless an
enemy champion is in view within 8 tiles, something hit us since last turn, or we stand at our
live farm camp (shopping happens on any turn spent on spawn). From the team Discord: The Mania
recall as soon as they can buy an emblem.

| Variant vs v8 | Head-to-head (34) | Regular bots (204 paired) | Top ladder (site, paired) |
|---|---|---|---|
| 150 gold (adopted) | 27/34 (p<0.01), +268/game | 196 vs 190 wins, +79/game (p<0.001) | 50 games: +74/game (t=2.18, p~0.03), 30 better / 15 worse, wins 2 vs 1 |
| next emblem affordable | 24/34 (p=0.02) | +45/game (p=0.04) | not run |
| 100 / 200 / 300 gold | 20 / 20 / 19 of 34 | +62 / +61 / +22 per game | not run |
| trip + Jungle emblem first (plan 14) | 24/34 | +70/game | not run |
| Jungle first, no trips | 18/34 | -2/game | not run |

Top ladder = exact rematches (same opponent versions and maps) of 10 match groups vs the top ten
(Fish, Mite, The Mania, Sneaky Bush Campers, Nyxius, procless, Univardisty of Shingtonwash,
new team(4), Boost, Nyx), traced v8 vs traced trip build.

With trips on, the recall health threshold was swept (vs trips at 35%): 20% worse (-37/game,
p=0.02); 25/30/40/45% no difference. Left at 35% (`RECALL_HP`).

## Camp Reward Collector (main, efa48d8) and Shop Trips Together

potionarcher's main changes (one champion collects camp rewards; wait beside respawning camps;
safe recall fix) vs v8: head-to-head 23/34 (p=0.06), regular bots +75/game (p<0.001), top ladder
49 paired games +106/game (t=3.41), 35 better / 9 worse. Merged with shop trips (this branch) vs
main: head-to-head 20/34, regular bots +26/game (p=0.14); main vs v8+trips on the top ladder +34/game
(t=1.15, n.s.). Both recover idle farming time, so their gains overlap. Merged vs main on the top
ladder (50 paired games): -31/game (t=-1.26, n.s.), 19 better / 28 worse. So shop trips are off by
default again (`SHOP_RECALL_GOLD` 0): this branch plays exactly like main.

468ed80 (potionarcher: recall to shop for the next emblem, `SHOP_RECALL`, on; fuel spike cuts) vs
efa48d8: head-to-head 18/34, regular bots +22/game (p=0.28), top ladder 50 paired games -4/game
(t=-0.20), 23 better / 21 worse. No difference; vs v8 on the top ladder +103/game (t=3.57). Live as v17.

## Monolith-Map Discount (tested, off) and Map Dump

`STARVED_ROUND_DISCOUNT`: a separate camp-value discount on gem-starved maps (stampede, contest).
vs current main (204 regular + 34 head-to-head): 0.97 -40/game (p=0.05), 12/34; 0.99 -99/game
(p<0.001), 15/34. Per map (12 each): stampede 0.97 wins 12/12, 1088 gems, ends ~round 1183 (main
11/12, 915, ~1789); contest 0.97 wins 3/12 (main 10/12). The two maps differ (`MAP_DUMP`):
stampede's gems are one 12-camp group (17,400 hp total) on the far side of an open, bushy map;
contest's are two single 20,000 hp monoliths in a walled map, equidistant from both spawns.

Held-out check (maps in `tools/maps_custom/`, made with the cambc map editor): stampede_mono (two
single monoliths, far right), stampede_center (the 12-camp herd moved to the centre), contest_side
(monoliths moved to the far corners, 42 from each spawn), contest_herd (each monolith replaced by
3 elk + 3 gem clusters). Wins of 14, 0.93 vs 0.97: 11 vs 10, 12 vs 12 (gems 964 vs 669), 13 vs 2,
12 vs 10. 0.97 helps only on the original stampede; neither gem-source type nor location predicts
it, so no per-class discount rule.

Why stampede likes 0.97 (traces vs starter-cpp): with 0.93 the team first targets the herd at
round 169 and spends 0-44% of farm turns on it (10,720 gold from clusters, done round 1614); with
0.97 it targets the herd from round 13, 80-90% of the time (3,370 gold, done round 1051).
`SEQ_CAMP_DAMAGE` (camp danger with camps killed weakest first, damage falling as they die) did not
rescue 0.97 on the other maps: over the 6 monolith-type maps x 14, main 71/84, 0.97+seq+limit 3
52/84, limit 6 46/84 (contest and contest_side still collapse). The seq model alone vs main:
head-to-head 18/34, regular bots +7/game (p=0.58). Off.

## Capture Stayer, Fight Value Check, Camp Crowding (tested, off)

vs v8: head-to-head of 34, then regular bots (204 paired, margin per game).
- `CAPTURE_STAYER` (one champion captures a cleared group, the rest move on): closest 17/34,
  -13 (n.s.); lowest health 14/34, -130 (p<0.001); weakest damage 9/34 (p=0.01), -210 (p<0.001).
- `FIGHT_EV` (fight only if P(win) x (enemy gems + kill value) > P(lose) x (2 x our gems + death
  cost); P from the team-fight call's measured accuracy): before starting 14/34, -62 (p<0.001);
  every turn 12/34, -91 (p<0.001).
- `CAMP_CROWD_ROUNDS` (enough of us at the team camp to kill it in K rounds: others go elsewhere):
  K=8 19/34, +29 (p=0.15); K=15 16/34, +20 (p=0.38).

## Shop

Default (`SHOP_PLAN` 13, `states/shop.h`): one emblem each of Power (main stat), Durability health
and Bruiser health, then Power shards of the main stat for the rest of the game. Main stat is focus
for Cassi, Stellar, Prism, Ren and Willow, strength for the others. This follows the two top
teams: summary data from 0-5 losses to each showed them buying Power shards of the main stat plus
a few emblems, clearing 2-3x as many camps and dealing 2-3x our monster damage.

Tested against the previous default, Bruiser health shards (plan 0), on all 17 maps, both sides,
vs v5, locker, main, opp-caller, opp-rush and starter-cpp (204 matches each), wins:
plan 13 157, Mite's order (10: Jungle, Power, Durability, Bruiser) 151, health only 149. An
earlier run of all plans with the combat trace on (238 matches each) agreed: 13 140, 10 142,
9 (13 with Willow on strength) 138, 11 (no Bruiser emblem) 138, 12 (Durability emblem then health)
133, health 130. With plan 10, per-champion camps (139) and higher gold weight in camp choice
(0.5: 145, 1.0: 145) were worse.
On the site (live as v6), rematches of v5's last ten unranked matches (same opponent versions
and maps): games won 29/50 vs 27/50 for v5, gems banked 38,378 vs 36,418; still 0/5 against each
of the top three (Fish, Mite, The Mania), whose main lead is farming speed.

Before that, eight plans were tested (544 matches: each vs the previous bot on both sides of
every map, plus starter-cpp and opp-rush); wins out of 68: Bruiser health 56, Bruiser strength
50, Bruiser focus 47, Home emblem first 47, Jungle first 47, Durability first 45, Monster first
44, Power focus 34 (focus for every champion). `SHOP_PLAN` switches plans in test builds (0 =
health only, 8 = Bruiser focus).

## Draft

Ren, Stellar, Cassi & Issac, Ryth, Tavi (`constants/draft_constants.h`). The first four are what
the top two teams draft; on four-champion maps only they are picked, so order matters. Tested
with the plan 13 shop, 204 matches each (17 maps, both sides, vs v5, locker, main, opp-caller,
opp-rush, starter-cpp), wins: this draft 177; same with Varg fifth (the top teams' exact draft)
174; Tavi first 173; Stellar first 172; Ren, Tavi, Cassi & Issac, Stellar, Ryth 171; old draft
with Tavi for Willow 172, with Ren for Willow 164, with Prism for Willow 160, with Ren for Ryth
141; old draft (Varg, Cassi & Issac, Willow, Stellar, Ryth) 157. Tribute at 60 or 80 gems
instead of 40 made no difference (172, 174). `DRAFT_PICKS` sets the whole draft in test builds.
On the site (live as v7), rematches of six of v5's unranked matches (same opponent versions and
maps): games won 13/30 vs 11/30 for v6 and 9/30 for v5; gems banked 19,580 vs 20,191 (v6);
still 0/15 against Fish, Mite and The Mania.

## Farming: Round Discount

Camp value is (gems + gold x 0.25) x `kRoundDiscount` ^ (walk + kill + capture rounds). Top teams
hit camps in 25-47% of rounds against our 14-25%; a per-turn trace (`STATE_TRACE`) showed our
champions walking in about half their farm turns. A lower discount prefers nearer camps. Sweep
with the current shop and draft (204 matches each, same six opponents as Draft), wins and mean
gems ours:theirs: 0.99 141 (873:597), 0.97 (old) 177 (949:508), 0.95 186 (968:456), 0.93 190
(960:419), 0.90 184 (914:415), 0.85 188 (895:404). Head-to-head vs 0.97: 0.93 21/34, 0.95 16/34.
Default 0.93. In the same sweep, per-champion camps (161) and a camp danger limit of 5 (166)
were worse; the teammate bonus only matters with per-champion camps.
On the site (live as v8, with the Draft and Shop changes), rematches of v5's last ten unranked
matches (same opponent versions and maps): games won 34/50 (v6 29, v5 27), gems banked 41,027
(v6 38,378, v5 36,476); first game won against Mite, and 25-35% more gems than v5 against each of
Fish, Mite and The Mania (still 1/15 games against them).

## Deaths, Splitting, Outnumbered Fights (tested, all off)

Traces of v8 against the top six (`STATE_TRACE`, joined with the site's death events) showed about
half our deaths happen mid-recall at low health with an enemy 1-4 tiles away, and the costly ones
follow a lost fight where a survivor picks up the dead teammates' gems. Site rematches are
deterministic (a rerun of v8's 30 top-six games reproduced all 30 exactly), so paired comparisons
on the same 30 games measure real effects for those games.

| Switch | Local, 204 paired vs v8 (wins, margin/game, p) | Head-to-head vs v8 | Site, 30 paired top-six games |
|---|---|---|---|
| `RECALL_HP_NEAR_ENEMY` 0.5 | 188 vs 190, -19, p=0.39 | 18/34 | margin +12/game (t=0.26), 8 better / 10 worse; wins 1 vs 1 |
| `CARRIER_AVOID_GEMS` 40 | 191 vs 190, -8, p=0.59 | 11/34 (p=0.06) | not run |
| both | 190 vs 190, -23, p=0.29 | 15/34 | margin -35/game (t=-1.07), 6 better / 12 worse; wins 0 vs 1 |
| `SPLIT_GROUPS` (Ren, Cassi & Issac / rest) | 182 vs 190, -43, p=0.05 | 14/34 | not run |
| `AVOID_OUTNUMBERED` | 181 vs 190, -79, p<0.001 | 14/34 | not run |
| split + avoid | 168 vs 190, -167, p<0.001 | 12/34 | not run |

None is better. Early recall changes single top-team games a lot in both directions (eight vs
The Mania became a 1039:501 win; pilgrimage vs Mite, our only win, became a loss) but not on
average. On elkring alone (12 matches) splitting banked 1006 vs 993 with the same wins.

## Camp Danger and Altar-Lock Memory (tested, all off)

Weighting: head-to-head vs the live bot and top-ladder site rematches count most; regular local
opponents (v5, locker, main, opp-caller, opp-rush, starter-cpp; 204 paired matches) count less.

| Switch | Head-to-head vs v8 | Regular bots: wins, margin/game vs v8 |
|---|---|---|
| `SOLO_DAMAGE_SHARE` 1.0 (camps taken alone) | 17/34 | 192 vs 190, -5 (p=0.69) |
| `SOLO_DAMAGE_SHARE` 1.5 | 16/34 | 191 vs 190, -3 (p=0.81) |
| `REAL_BACKUP` (solo choice counts teammates near the camp) | 18/34 | 194 vs 190, +35 (p=0.08) |
| `REAL_BACKUP` + solo 1.5 | 19/34 | 189 vs 190, +22 (p=0.30) |
| `ALTAR_MEMORY` (bank at altars not seen locked) | 17/34 | 186 vs 190, -27 (p=0.13); vs locker 27/34 (v8 33/34) |

`REAL_BACKUP` is the only candidate; it has not been run against the top ladder yet. Walking to a
farther unlocked altar costs more than breaking a lock (a few hits), so `ALTAR_MEMORY` stays off.

## Idle Recall and Pathing Tie-Break (tested, no change)

- Idle recall (`IDLE_RECALL_HP`, `states/recall.cpp`): with nothing to farm, a hurt champion
  recalls. Traces showed ~16 such recalls a game locally (at ~81% health), but only 0-1 a game
  against top teams. Recalling only below 70% / 50% / never: 190 / 188 / 189 of 204 vs 190 for
  the current rule; head-to-head 17, 15, 16 of 34. Left at 1.0 (whenever hurt).
- Straight tie-break (`STRAIGHT_TIE_BREAK`, `nav/pathfinding.cpp`): equally short next steps
  are taken in tile order, so walks drift to low y and follow the map edge. Same number of
  steps either way; preferring straight steps: 189/204 vs 190, head-to-head 16/34. Left off.
- `STATE_TRACE` logs each turn's state, position, gems, health, visible enemies, nearest enemy
  and farm target (site replay bundles include our stdout).

## Farming: Camp Danger and Monolith Maps

Ranked play showed a 0 : 1080 loss on stampede. On stampede and contest the only gems are in
monoliths, which the farm logic always rejected as too dangerous (expected damage per champion
over `kMaxCampDamageShare` = 0.6 of its health), so the team farmed gold all game and banked
nothing (0 gems in 36/38 local stampede games, 33/38 contest). Monoliths keep their damage while
we recall and heal, so they can be worn down.
- `kMaxCampDamageShare` is now 3.0 (0.6 was too cautious on every map).
- Gem-starved maps (no gem group a single champion could take within that limit: contest,
  stampede, rows) take gem groups with `kMaxCampDamageShareGemStarved` (no limit). The check is
  re-made every round from the champion's current health and damage, which grow with shards, so
  on a borderline map (rows) a champion can stop counting the map as starved mid-game.
Tested on 340 matches (17 maps x 6 opponents, plus head-to-head): wins 74/102 vs 67/102 before,
mean gems 905 vs 786, zero-gem games 4 vs 9; gem-starved maps 13/18 vs 8/18 (gems 771 vs 328);
other maps 61/84 vs 59/84; head-to-head 20/34 with gems 877 vs 703. No faults.

## Farming: Opening and Per-Champion Camps

- Opening (`kOpeningClosestCamp`): at match start each champion farms the camp group with the
  shortest walk from where it stands until that group is cleared and starts respawning (or
  `kOpeningMaxRounds` pass), then farms normally. 136 matches: head-to-head vs the old opening
  ahead in 16/34 with mean banked gems 771 vs 732 (ahead by 7 at round 100, 21 at round 200);
  against 3 opponents ahead in 36/51 (old 31/51). A small, consistent gain.
- Per-champion camps (`kPerChampionCamps`): no shared team target; each champion picks its own
  best group, counting teammates within `kHelpRadius` (seen, or from position reports) as
  helpers plus `kCampTeammateBonus` each. Groups within `kNearSpawnWalk` of spawn may use
  `kMaxCampDamageShareNearSpawn`. Tested (85-136 matches per setting): letting near-spawn camps
  cost up to 100% health hurt (ahead 31/51 vs 36/51 for the team target), so that limit is 0.6
  like everywhere else; with it, per-champion choice is about even with the team target
  (head-to-head 20/34, against 3 opponents 34/51 vs 36/51). It banks more early (round 100)
  and about the same later. Bonus 20 instead of 6 did not help. In the full regression (all
  features on, 272 + 340 matches) it was the one feature that cost gems: switching it off
  raised banked gems by ~120-200 a game against starters and the rush opponent (e.g. 9/17 ->
  14/17 wins vs opp-rush), so it is now off by default.

## Waiting on Camp Fights

`kWaitForCampFights` (attack state): an enemy fighting a camp (an awake camp within its reach)
is attacked only once that camp group is at or below `kWaitCampHealthFraction` (0.3) of its
health or dead, or while we are not the champion closest to the camp (camps hit the closest;
when closing in we only step to tiles farther from the camp than the enemy). Until then we hold,
stepping back if we are the closest. A teammate's fight call overrides it. It triggers often
(476 waiting decisions in 17 matches) but did not measurably change results: 136 matches,
head-to-head ahead in 15/34, against 3 opponents ahead in 31/51 vs 34/51 without it (within
noise). With it switched off, play is identical to before (replays checked).

## Enemy Gems

`enemy_gems(enemy, match)` (`core/enemy_gems.h`): an enemy's carried gems are exact while it is
visible (`champion::gem_count` works on enemies; 76/76 readings matched the enemy's own count in
a self-play probe). Out of sight it returns the last reading and how many rounds old it is. It
also gives the gem value of the camp group the enemy is fighting (a damaged camp within its
reach), so `total()` is what it holds once it captures that group. Nothing calls it yet.

## Grouping: Together vs Separate (no change)

Three alternatives to the current farming (one team target, solo when a solo camp is worth
more) were A/B tested (884 matches). Mean teammates within 5 tiles: together 3.03, current 1.63,
combination 1.09, separate 0.94.
- Together (`kStayTogether`): team target only, regroup to the leader beyond `kGroupRadius`.
  Clearly worse: 8/34 vs the current bot, 57/85 vs 64/85 against other opponents.
- Separate (`kPerChampionCamps` with `kCampTeammateBonus` -6): 19/34 vs current; wins vs
  others 168/221 combined vs current 167/221.
- Combination (separate farming + converge on fight calls, every 3 rounds): 19/34 vs current;
  172/221 vs 167/221; vs separate 17/34.
Separate and combination are within noise of the current bot, so farming is unchanged.

Bunching only when enemies are near (`kBunchMode`, 357 more matches) did not help either. The
current bot already has 2.05 teammates within 5 tiles on turns with an enemy within 8.
- Rally (1): a champion seeing an enemy within `kRallyTrigger` broadcasts a rally point (bytes
  6-7); farming teammates within `kRallyRange` gather there. 10/34 vs current, 57/85 vs others
  (current 64/85). A stronger rally (range 25, radius 2, 6 rounds): 12/34, 62/85.
- Local (2): a farming champion seeing an enemy closes up with its nearest visible teammate.
  15/34 vs current, 64/85 vs others (same as current).
All off; bytes 6-7 stay free unless rally mode is switched on.

## Channel Safety and Pursuit (both off)

- Tribute safety (`brain/channel_safety.cpp`, the team's `channel_still_safe()` hook from main):
  `kTributeSafetyPlan` 1 cancels a tribute when hit since our last turn, 2 when the enemies in
  reach could deal our remaining health before it completes; recalls are left alone (damage
  cancels them anyway). Tributes last only 1-4 turns, so it almost never triggers (1 cancel in 4
  matches vs opp-rush for plan 1, none for plan 2): off, since no effect could be measured.
- Pursuit (`states/attack.cpp`, `kPursuePlan`): 1 attacks a recalling/tributing enemy in reach
  first; 2 also chases one within `kPursueRange` carrying at least `kPursueMinGems` while we have
  at least `kPursueMinHealth`. 612 matches: plan 1 68/102 vs 69/102 off; plan 2 tied with off
  on fresh opponents (103/136 each) and 51/68 vs 52/68 on the first opponents (its 20/34
  head-to-head is one deterministic set of games). Off.

## Dash

`skills/dash_move.h`: `dash_towards(goals)` dashes to the legal landing tile closest to the goal
when that beats one ordinary step. `kDashPlan`: 0 never, 1 close in on the attack target, 2 toward
the safe tile when retreating to recall (default), 3 both. With our draft only Issac can dash and
no plan changed results (47-48/68). With a dash draft (Vendetta, Vesper, Ren, Varg,
Cassi+Issac), 544 + 408 matches: escape 123/204 wins, close-in 115/203, none 112/204, both 37/68
in the first batch. Escape is on; close-in did not hold up on fresh opponents.

## Our Altar Locks (off)

`skills/altar_locks.h`, `kAltarLockPlan` (0 = off): 1 locks any unlocked altar within 2 tiles,
2 locks an altar where a visible enemy is tributing (placing a lock cancels their channel), 3
both; plans 1-3 buy one lock (10 gold) before shards when not carrying one. Tested on 408
matches (each plan vs the current bot both sides, plus the previous bot, opp-rush, locker and
main): wins out of 102 were 72 off, 69 plan 1, 71 plan 2, 69 plan 3; head-to-head vs the
current bot 13/34 and 15/34. Locks cut the opponent's banked gems (vs main 402-542 vs 649) but
the gold they cost buys fewer shards and the win rate did not improve, so they stay off.

## Enemy Altar Locks

An enemy altar lock blocks our tributes at that altar until three basic attacks break it (our own
locks do not block us). The tribute state treats locked altars as normal destinations: at one it
attacks the lock, then tributes once it is gone. Tested against a test opponent that places a
lock on every altar it can (`TEST_ALTAR_LOCKER`), 68 matches over 17 maps and both sides: ahead at
the end in 16/34 (old behaviour 6/34), mean banked gems 705 vs 652 (old 447 vs 765). Without
locks, play is unchanged (identical replays). `BREAK_ALTAR_LOCKS=false` restores the old
behaviour in test builds; `SCORE_TRACE` logs banked gems.

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

**Teammate positions (comms/ally_positions.h).** Bytes 4-5 of shared memory carry one
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

**Fight calls.** `call_team_fight(target)` writes the shared header word (enemy, round, last
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

