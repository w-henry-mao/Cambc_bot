#!/usr/bin/env python3
"""How retreats ended, from RECALL_END lines (builds with RECALL_TRACE=true).

  recall_report.py <replay-dir> [<replay-dir> ...]

A retreat starts when the recall state runs away from spawn and ends when the champion reaches
spawn (home), dies, or 120 rounds pass (gave_up). Grouped by our build and opponent, taken from
replay names <ours>__<opponent>__<map>__<side>.repl26a. Our bot is team A in these batches.
"""
import collections, pathlib, re, sys, zipfile

def kv(line, key):
    m = re.search(rf"\b{key}=([-\w.]+)", line)
    return m.group(1) if m else None

def main(dirs):
    stats = collections.defaultdict(lambda: collections.Counter())
    rounds = collections.defaultdict(list)
    for d in dirs:
        for path in sorted(pathlib.Path(d).glob("*.repl26a")):
            parts = path.stem.split("__")
            ours, opp = (parts[0], parts[1]) if len(parts) >= 2 else ("?", "?")
            with zipfile.ZipFile(path) as z:
                text = z.read("team-a.stdout").decode("utf-8", "replace")
            for line in text.splitlines():
                if "RECALL_END" not in line:
                    continue
                line = line[line.index("RECALL_END"):]
                how = kv(line, "how")
                for key in (ours, f"{ours} vs {opp}"):
                    s = stats[key]
                    s["n"] += 1
                    s[how] += 1
                    s["cancels"] += int(kv(line, "cancels") or 0)
                    if how == "home":
                        rounds[key].append(int(kv(line, "rounds")))
    print(f"{'':34} {'retreats':>8} {'home':>6} {'died':>6} {'gave up':>8} {'cancels/retreat':>16} {'median rounds home':>19}")
    for key in sorted(stats, key=lambda k: (" vs " in k, k)):
        s = stats[key]
        n = s["n"]
        r = sorted(rounds[key])
        med = r[len(r) // 2] if r else 0
        print(f"{key:34} {n:8} {100*s['home']/n:5.1f}% {100*s['died']/n:5.1f}% {100*s['gave_up']/n:7.1f}% "
              f"{s['cancels']/n:16.2f} {med:19}")

if __name__ == "__main__":
    main(sys.argv[1:] or ["."])
