# Combat rules (measured)

Measured with the probe bot in `tools/bots/probe` (`python3 tools/run_probe.py <a> <b> packs <rounds>`)
on cambc 2.1.6. One champion per side walks to the map centre; everything else stays home.
The fight model in `src/combat/` implements exactly these rules.

| # | Question | Observed | Rule |
| --- | --- | --- | --- |
| 1 | Basic attack damage | Vendetta (Strength 70) on Ryth: 1200 → 1130 → 1060 … every hit exactly 70 | A basic attack deals Strength, no rounding |
| 2 | Spell damage | Stellar slot 0: `damage_amount` 200, actual 200. Slot 1 (AoE, radius 1) on a lone target: predicted 140, actual 140 | Spell = modifier × Focus; AoE hits a lone target in full; `damage_amount(target)` is exact |
| 2b | Cast and attack in one turn | After a walk-excluding cast: `mv=0 at=1` | A cast blocks walking (if `excludes_walking`) but not the basic attack |
| 3 | Ranged walk then attack | "Attack already used" | Ranged: an ordinary move uses up the attack (and vice versa) |
| 3b | Ranged dash then attack | Vesper dash (2 tiles) then attack: both ok | A dash does not use up the ranged walk/attack allowance |
| 3c | Ranged walk, reset, attack | Vesper: move ok, reset ok, attack ok (one turn) | An attack reset restores the attack even after a ranged walk |
| 4 | Melee walk + attack, reset + attack | Vendetta: walk + attack ok; attack, reset, attack = two hits | Melee walks and attacks; a reset gives a second attack |
| 4b | Reset before attacking | "An available attack cannot be reset" | A reset only works after the attack is used |
| 5 | One-round stun | Cassi stuns Ryth after Ryth's turn in round 14; Ryth's round-15 turn: `mv=0 at=0 ca=0`; round 16 normal | A stun removes the target's whole next turn |
| 5b | Stun spell and walking | Cassi's stun (`excludes_walking=0`): `mv=1 at=1` after cast | Walk-compatible spells leave walk and attack available |
| 6 | Shield | Varg shield 250 (2.5 × 100); hits drain shield first, overflow goes to health | Shield absorbs first, then health |
| 6b | Ren retaliation | Attacker loses 20 (0.2 × Focus 100) on every hit, including the killing hit (13 hits → 260) | Retaliation per damaging hit, also on the lethal one |
| 7 | Cooldown counting | Cast in round 19 shows 8, then 7, 6 … 0 in round 27 | Ready again on the owner's 8th turn after the cast |
| 8 | Turn order | A first in round 13, B first in round 14, A first in round 15 | The starting team swaps every round |
| — | Passive gold | Gold stays 0 for 60 rounds | No passive gold; gold comes from camps and kills |
| 9 | Shield expiry | Varg duel: a shield on 0 turns left still absorbed the next hits, then vanished | Zero timers resolve at the owner's turn start: a shield on 0 lasts until then |
| 10 | Turn order between two fighters | Willow duel: the enemy acted exactly once between each of our turns | Turns alternate team by team, champion by champion (A1 B1 A2 B2 …) and the starting team swaps each round. Only fighters at the same position in their team's order get double turns; otherwise it is strictly us, them, us, them |
| 11 | `move_towards(enemy tile)` | Our Ryth never closed in on a ranged enemy | The target tile is occupied, so the call always returns false (see `kAttackStateClosesIn`) |

## Checked by exact duels (tools/duel_harness.py, 132/132 exact)

Ryth's area stun (`aoe_damage` with duration 1) stuns like `cc`; Marcella's enhanced attack
stun and stun immunity; Varg's shield over its full lifetime; Ren's retaliation; dashes.

## Not measured yet (modelled from docs/GameRules.md)

- Durability emblem: −10% incoming damage. It needs 150 gold from camps, so no probe or duel
  has used it yet.
