#!/usr/bin/env python3
"""Path-cache hit rate and fuel from trace builds: path_cache_stats.py <replay-dir>...

Reads PATH_TURN lines (one fast prediction per turn during a tracked fight: hit or miss and its
fuel) and PATH_CHECK lines (VERIFY_PATH_CACHE builds: hit compared with a fresh calculation).
Fuel figures are only meaningful at the default fuel quantum with verification off.
"""
import re, statistics, sys, zipfile, pathlib
# Usage: pathstats.py <replay dir>...  Counts PATH_TURN hits/misses (+fuel) and PATH_CHECK agreements.
for d in sys.argv[1:]:
    hits, misses, hit_fuel, miss_fuel, checks, bad = 0, 0, [], [], 0, 0
    for f in sorted(pathlib.Path(d).glob("*.repl26a")):
        z = zipfile.ZipFile(f)
        for team in ("team-a", "team-b"):
            text = "".join(ch if ch.isprintable() or ch == "\n" else " " for ch in z.read(f"{team}.stdout").decode("utf-8", "replace"))
            for line in text.splitlines():
                if "PATH_TURN" in line:
                    m = re.search(r"hit=(\d) fuel=(\d+)", line)
                    if not m: continue
                    fuel = int(m.group(2))
                    if fuel > 10**12: continue
                    if m.group(1) == "1": hits += 1; hit_fuel.append(fuel)
                    else: misses += 1; miss_fuel.append(fuel)
                elif "PATH_CHECK" in line:
                    checks += 1; bad += "same=0" in line
    def summ(v): return f"median {statistics.median(v):.0f}, p95 {sorted(v)[int(.95*len(v))-1] if v else 0}, max {max(v)}" if v else "n/a"
    n = hits + misses
    print(f"{pathlib.Path(d).name}: per-turn calls {n}, path hits {hits} ({100*hits/max(1,n):.0f}%); "
          f"hit fuel {summ(hit_fuel)}; miss fuel {summ(miss_fuel)}; verify checks {checks}, disagreements {bad}")
