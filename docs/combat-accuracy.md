# Fight calculator: accuracy and cost

Measured on the `fight-calculator` branch with cambc 2.1.6. Commands are in `tools/`.

## Exact duels (`tools/duel_harness.py`)

Our bot (duel test build: one champion walks out and fights with the normal attack code,
recall off) against `duel-aggressor` (plays the model's max-aggression plan) on `packs`, from
both sides. Every one of our six drafted champions against all eleven enemy picks: **132
clean duels, 132 exact** (same winner, same remaining health), and the full version's worst
case was never above the real result.

## In normal matches (`tools/accuracy_runs.py`, `tools/accuracy_report.py`)

Trace builds at four aggression levels, all 17 maps, both sides against `starter-cpp` plus
self-play. A fight counts when it finished (someone died), was a clean one-on-one (no third
champion within 4 tiles, no awake camp in range, no damage or stun the enemy could not have
dealt) and our bot stayed in its attack state.

| Variant | Clean fights | Worse than worst case | Worst case said win, we lost | Said loss, we won |
| --- | --- | --- | --- | --- |
| base | 33 | 0 | 0 | 4 |
| no recall | 98 | 0 | 0 | 14 |
| no recall, reach +2 | 28 | 0 | 0 | 15 |
| reach +4 | 10 | 0 | 0 | 4 |

"Said loss, we won" is expected: those opponents are weaker than a maximally aggressive one.
About 95% of fights in full games are not clean one-on-ones and are not counted.

## Fuel (default fuel, 34 matches vs `starter-cpp`, 1,199 predictions)

| | median | 95th percentile | max |
| --- | --- | --- | --- |
| Reading game state | 2,538 | 3,531 | 9,448 |
| Fast version, total | 12,277 | 19,708 | 25,415 |
| Full version, total | 19,694 | 45,820 | 53,202 |

The fast version stops at `kFastSimFuelBudget` (15,000) of simulation and answers
"unresolved" (12% of calls). The full version never ran out of fuel: it gets the fuel left
minus `kFullFuelReserve` (25,000) and stops searching stun timings at 60% of that.

## Known limits

- One-on-one only (plus the rough two-against-one check). Enemies out of vision are unknown.
- Distance is straight-line (Chebyshev): fights around walls are modelled as open ground.
- Durability emblem (−10%) is from the rules, not yet measured.
- Our side mirrors today's attack state, which never closes in (`kAttackStateClosesIn`).
