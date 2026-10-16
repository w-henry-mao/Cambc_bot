#!/usr/bin/env python3
"""Task 10 runs: trace builds of our bot at several aggression levels, every map, both sides,
against starter-cpp and in self-play. Then accuracy_report.py on the results.

  accuracy_runs.py [--jobs 8] [--fuel-quantum 1000000]
"""
import argparse, pathlib, shutil, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
WS = pathlib.Path.home() / "cambc-ws"
VARIANTS = {
    "acc-base": [],
    "acc-norecall": ["RECALL_SCORE=0"],
    "acc-norecall-reach2": ["RECALL_SCORE=0", "REACH_BONUS=2"],
    "acc-reach4": ["REACH_BONUS=4"],
}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", default="8"); ap.add_argument("--fuel-quantum", default="1000000")
    ap.add_argument("--variants", default="", help="comma-separated subset of variant names")
    ap.add_argument("--extra", default="", help="extra NAME=VALUE overrides for every variant, comma-separated")
    args = ap.parse_args()
    chosen = args.variants.split(",") if args.variants else list(VARIANTS)
    maps = sorted((WS / "maps").glob("*.map26a"))
    for name, overrides in VARIANTS.items():
        if name not in chosen: continue
        subprocess.run([sys.executable, str(ROOT / "tools/make_test_variant.py"), name, *overrides, *[x for x in args.extra.split(",") if x]], check=True, capture_output=True)
        out = WS / "replays" / name
        if out.exists(): shutil.rmtree(out)
        out.mkdir(parents=True)
        lines = []
        for m in maps:
            lines.append(f"bots/{name} bots/starter-cpp maps/{m.name} replays/{name}/A-{m.stem}.repl26a")
            lines.append(f"bots/starter-cpp bots/{name} maps/{m.name} replays/{name}/B-{m.stem}.repl26a")
            lines.append(f"bots/{name} bots/{name} maps/{m.name} replays/{name}/S-{m.stem}.repl26a")
        (WS / f"{name}.txt").write_text("\n".join(lines) + "\n")
        r = subprocess.run([str(WS / ".venv/bin/cambc"), "run-batch", f"{name}.txt", "--jobs", args.jobs,
                            "--fuel-quantum", args.fuel_quantum, "--timeout", "900"], cwd=WS, capture_output=True, text=True)
        print(name, r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr[-500:])
        print(subprocess.run([sys.executable, str(ROOT / "tools/accuracy_report.py"), str(out)], capture_output=True, text=True).stdout)

if __name__ == "__main__":
    main()
