#!/usr/bin/env python3
"""Exact duel test for the fight calculator.

For every pairing of one of our drafted champions against every enemy champion, our bot
(duel-mode test build: one champion walks out and fights with the normal attack code, recall
off) meets the duel-aggressor bot (plays the model's max-aggression plan) on `packs`, from both
sides. Because matches are deterministic, a clean duel must end exactly as the fast prediction
says. Prints a table and a summary.

  duel_harness.py [--jobs 8] [--rounds 200]
"""
import argparse, pathlib, re, shutil, subprocess, sys, zipfile
ROOT = pathlib.Path(__file__).resolve().parent.parent
WS = pathlib.Path.home() / "cambc-ws"
OUR_TYPES = {10: "varg", 3: "cassi", 4: "issac", 8: "willow", 5: "stellar", 0: "ryth"}
# Our champions drafted 4th/5th do not play on small maps: duel builds draft them first.
FIRST_PICK = {5: 4, 0: 0}
# enemy draft pick -> champion type of the fighter (Cassi & Issac pick: Cassi fights)
ENEMY = {0: 0, 1: 1, 2: 2, 3: 3, 4: 5, 5: 6, 6: 7, 7: 8, 8: 9, 9: 10, 10: 11}

def install_aggressor(pick, ctype):
    dst = WS / "bots" / f"agg-{pick}"
    if (dst / "src").exists(): shutil.rmtree(dst / "src")
    (dst / "src" / "combat").mkdir(parents=True)
    (dst / "src" / "core").mkdir()
    (dst / "src" / "constants").mkdir()
    for f in (ROOT / "src/combat").iterdir():
        if f.name.startswith(("sdk_trace", "sdk_duel_mode")): continue
        shutil.copy(f, dst / "src/combat" / f.name)
    for f in ("roster.h", "roster.cpp"): shutil.copy(ROOT / "src/core" / f, dst / "src/core" / f)
    for f in ("combat_constants.h", "test_hooks.h"): shutil.copy(ROOT / "src/constants" / f, dst / "src/constants" / f)
    shutil.copy(ROOT / "tools/bots/duel-aggressor/main.cpp", dst / "src/main.cpp")
    (dst / "src/agg_config.h").write_text(f"#pragma once\nconstexpr int kAggPick = {pick}, kAggType = {ctype};\n")

def kv(line, key):
    m = re.search(rf"\b{key}=([-\w.]+)", line)
    return m.group(1) if m else None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", default="8"); ap.add_argument("--rounds", default="200")
    ap.add_argument("--map", default="packs")
    ap.add_argument("--types", default="", help="comma-separated champion types of ours (default: all drafted)")
    ap.add_argument("--out", default="duel")
    args = ap.parse_args()
    types = [int(x) for x in args.types.split(",")] if args.types else list(OUR_TYPES)
    for t in types:
        subprocess.run([sys.executable, str(ROOT / "tools/make_test_variant.py"), f"duel-{t}", "DUEL_MODE=true",
                        f"DUEL_FIGHTER_TYPE={t}", "RECALL_SCORE=0", "FAST_SIM_FUEL_BUDGET=1000000"]
                       + ([f"DUEL_FIRST_PICK={FIRST_PICK[t]}"] if t in FIRST_PICK else []), check=True, capture_output=True)
    for p, ct in ENEMY.items(): install_aggressor(p, ct)
    out = WS / "replays" / args.out
    if out.exists(): shutil.rmtree(out)
    out.mkdir(parents=True)
    lines = []
    for t in types:
        for p in ENEMY:
            lines.append(f"bots/duel-{t} bots/agg-{p} maps/{args.map}.map26a replays/{args.out}/{t}-{p}-A.repl26a")
            lines.append(f"bots/agg-{p} bots/duel-{t} maps/{args.map}.map26a replays/{args.out}/{t}-{p}-B.repl26a")
    (WS / f"{args.out}.txt").write_text("\n".join(lines) + "\n")
    # Duel builds resign once their fight ends. A large fuel quantum keeps the trace's two
    # predictions from starving the bot's own actions on the contact turn (fuel cost is measured
    # separately at the default quantum).
    subprocess.run([str(WS / ".venv/bin/cambc"), "run-batch", f"{args.out}.txt", "--jobs", args.jobs, "--rounds", "450",
                    "--fuel-quantum", "1000000", "--timeout", "180"], cwd=WS, capture_output=True, text=True)
    rows, exact, bound_ok, total = [], 0, 0, 0
    for f in sorted(out.glob("*.repl26a")):
        t, p, side = f.stem.split("-")
        ours = "team-a" if side == "A" else "team-b"
        with zipfile.ZipFile(f) as z:
            text = z.read(f"{ours}.stdout").decode("utf-8", "replace")
        text = "".join(ch if ch.isprintable() or ch == "\n" else " " for ch in text)
        pred = next((l for l in text.splitlines() if "FIGHT_PRED" in l), None)
        end = next((l for l in text.splitlines() if "FIGHT_END" in l), None)
        name = f"{OUR_TYPES[int(t)]} vs {kv(pred, 'enemy') if pred else ENEMY[int(p)]} ({side})"
        if not pred or not end:
            rows.append((name, "no fight", "", "", "")); continue
        how = kv(end, "how"); actual_win = how == "enemy_died"
        actual_hp = float(kv(end, "our_hp")) if how != "we_died" else 0.0
        fast_win = kv(pred, "fast_win") == "1"; fast_hp = float(kv(pred, "fast_hp"))
        full_hp = float(kv(pred, "full_hp"))
        clean = kv(pred, "clean") == "1" and kv(end, "clean") == "1" and kv(end, "policy_ok") == "1"
        if how == "separated" or not clean or kv(pred, "fast_resolved") != "1":
            rows.append((name, f"skip ({how}, clean={int(clean)}, resolved={kv(pred, 'fast_resolved')})", "", "", "")); continue
        total += 1
        match = fast_win == actual_win and abs(fast_hp - actual_hp) < 0.5
        exact += match
        bound_ok += full_hp <= actual_hp + 0.5
        rows.append((name, "EXACT" if match else "MISMATCH", f"pred win={int(fast_win)} hp={fast_hp:g}",
                     f"actual win={int(actual_win)} hp={actual_hp:g}", f"full_hp={full_hp:g}"))
    for r in rows: print(" | ".join(x for x in r if x))
    print(f"\nclean resolved duels: {total}, exact: {exact}, full worst-case <= actual: {bound_ok}")
    return 0 if exact == total else 1

if __name__ == "__main__":
    sys.exit(main())
