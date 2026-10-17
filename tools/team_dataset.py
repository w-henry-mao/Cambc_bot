#!/usr/bin/env python3
"""Turn TEAM_PRED / TEAM_END logs into one line per estimate for tools/team_resim.cpp.

  team_dataset.py <out.tsv> <replay-dir> [<replay-dir> ...]

Columns (tab-separated): replay name, our build, opponent, map, ally deaths, enemy deaths,
allies and enemies that joined later (-1 in older logs), then the TEAM_PRED line itself (model
input and the calls made in the match) with the TEAM_END death rounds appended. Replay names follow
<ours>__<opponent>__<map>__<side>.repl26a; other names give "?".
"""
import pathlib, re, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from team_fight_report import rows, kv

def main(out, dirs):
    n = 0
    with open(out, "w") as f:
        for d in dirs:
            for path in sorted(pathlib.Path(d).glob("*.repl26a")):
                parts = path.stem.split("__")
                ours, opp, mp = (parts + ["?", "?", "?"])[:3] if len(parts) >= 3 else ("?", "?", "?")
                for pred, ad, ed, end in rows(path):
                    ja, je = kv(end, "joined_allies") or "-1", kv(end, "joined_enemies") or "-1"
                    # Death-round lists are "/"-separated: take the whole token, not kv()'s word.
                    extra = "".join(f" {m.group(0).strip()}" for k in ("ally_death_rounds", "enemy_death_rounds")
                                    for m in [re.search(rf" {k}=\S+", end)] if m)
                    f.write(f"{path.name}\t{ours}\t{opp}\t{mp}\t{ad}\t{ed}\t{ja}\t{je}\t{pred}{extra}\n")
                    n += 1
    print(f"{n} estimates -> {out}")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
