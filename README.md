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
