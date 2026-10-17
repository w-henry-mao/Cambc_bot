#!/usr/bin/env python3
"""In-match check of the team-fight estimate (combat/team_fight.h).

  team_fight_report.py [--by-opponent] <replay-dir> [<replay-dir> ...]

Reads TEAM_PRED / TEAM_END lines (builds with TEAM_FIGHT_TRACE=true) from every .repl26a bundle,
both teams. Each estimate is paired with the TEAM_END of the same champion and start round; the
outcome is the deaths among the champions in the estimate during the next 20 rounds. Per call
(fight / avoid / unsure) it prints how often we lost fewer champions than they did, the same,
or more. --by-opponent adds breakdowns of the quick version by opponent (from replay names
<ours>__<opponent>__<map>__<side>.repl26a), by our build, by fight size and by numbers.
"""
import collections, pathlib, re, sys, zipfile

def kv(line, key):
    m = re.search(rf"\b{key}=([-\w.]+)", line)
    return m.group(1) if m else None

def rows(path):
    with zipfile.ZipFile(path) as z:
        for team in ("team-a", "team-b"):
            try:
                text = z.read(f"{team}.stdout").decode("utf-8", "replace")
            except KeyError:
                continue
            preds = {}
            for line in text.splitlines():
                if "TEAM_PRED" in line:
                    line = line[line.index("TEAM_PRED"):]
                    preds[(kv(line, "me"), kv(line, "r"))] = line
                elif "TEAM_END" in line:
                    line = line[line.index("TEAM_END"):]
                    start, me = kv(line, "start"), kv(line, "me")
                    key = (me, start) if me else next((k for k in preds if k[1] == start), None)
                    pred = preds.pop(key, None) if key else None
                    if pred:
                        yield pred, int(kv(line, "ally_deaths")), int(kv(line, "enemy_deaths")), line

def table(title, by_call):
    print(f"{title}\ncall     n   we lost fewer   same   more")
    for call in ("fight", "unsure", "avoid"):
        b, e, w = by_call[call]
        n = b + e + w
        if n:
            print(f"{call:7} {n:4}   {b:5} ({100*b//n:3}%)  {e:4}  {w:4} ({100*w//n:3}%)")

def med_max(xs):
    xs = sorted(x for x in xs if x >= 0)   # -1: not measured (the turn ran out of fuel, or skipped)
    return f"{xs[len(xs)//2]:6} / {xs[-1]:6}" if xs else "     - /      -"

def main(dirs):
    # Fights where nobody died tell nothing about who would have won: counted apart.
    # "both": only estimates where both versions ran (the round one is skipped on short-fuel
    # turns, which are often the busy ones), so the two are compared on the same fights.
    models = {k: collections.defaultdict(lambda: [0, 0, 0]) for k in ("round_both", "quick_both", "quick_all")}
    fuel = collections.defaultdict(lambda: ([], [], []))
    quiet = 0
    agree = total = 0
    for d in dirs:
        for path in sorted(pathlib.Path(d).glob("*.repl26a")):
            for pred, ours, theirs, _end in rows(path):
                n = int(kv(pred, "allies")) + int(kv(pred, "enemies"))
                f = fuel[n]
                f[0].append(int(kv(pred, "setup_fuel")))
                f[1].append(int(kv(pred, "est_fuel")))
                if kv(pred, "quick_fuel") is not None: f[2].append(int(kv(pred, "quick_fuel")))
                rc, qc = kv(pred, "call"), kv(pred, "quick_call")
                both = rc not in (None, "skip") and qc is not None
                if both:
                    total += 1
                    agree += rc == qc
                if ours == 0 and theirs == 0:
                    quiet += 1
                    continue
                k = 0 if ours < theirs else 1 if ours == theirs else 2
                if both:
                    models["round_both"][rc][k] += 1
                    models["quick_both"][qc][k] += 1
                if qc: models["quick_all"][qc][k] += 1
                elif rc not in (None, "skip"): models["round_both"][rc][k] += 1   # older logs: round only
    table("round version, estimates where both ran", models["round_both"])
    if any(models["quick_both"].values()):
        table("\nquick version, same estimates", models["quick_both"])
        table("\nquick version, all estimates", models["quick_all"])
    print(f"\n{quiet} estimates with no deaths on either side left out; versions agree on {agree}/{total}")
    print("\nfuel by champions in the estimate (median / max): setup, round version, quick version")
    for n in sorted(fuel):
        s, e, q = fuel[n]
        print(f"  {n:2}: setup {med_max(s)}   round {med_max(e)}   quick {med_max(q)}   ({len(s)} calls)")

def compact(title, groups):
    # One line per group: per call, n and the share where we lost fewer / more champions.
    print(f"\n{title}")
    print(f"  {'':22} {'fight: n  fewer  more':>24} {'unsure: n  fewer  more':>25} {'avoid: n  fewer  more':>24}")
    for name in sorted(groups):
        by_call = groups[name]
        cells = []
        for call in ("fight", "unsure", "avoid"):
            b, e, w = by_call[call]
            n = b + e + w
            cells.append(f"{n:5} {100*b//n:4}% {100*w//n:4}%" if n else f"{'-':>5} {'':5} {'':5}")
        print(f"  {name:22} {cells[0]:>24} {cells[1]:>25} {cells[2]:>24}")

def breakdowns(dirs):
    new = lambda: collections.defaultdict(lambda: [0, 0, 0])
    by_opp, by_ours, by_size, by_odds = (collections.defaultdict(new) for _ in range(4))
    disagree = collections.defaultdict(lambda: [0, 0, 0])
    for d in dirs:
        for path in sorted(pathlib.Path(d).glob("*.repl26a")):
            parts = path.stem.split("__")
            ours_name, opp = (parts[0], parts[1]) if len(parts) >= 2 else ("?", "?")
            for pred, ours, theirs, _end in rows(path):
                qc = kv(pred, "quick_call")
                if not qc or (ours == 0 and theirs == 0):
                    continue
                k = 0 if ours < theirs else 1 if ours == theirs else 2
                a, e = int(kv(pred, "allies")), int(kv(pred, "enemies"))
                by_opp[opp][qc][k] += 1
                by_ours[ours_name][qc][k] += 1
                by_size["3 champions" if a + e == 3 else "4-5 champions" if a + e <= 5 else "6+ champions"][qc][k] += 1
                by_odds["we outnumber" if a > e else "even numbers" if a == e else "we are outnumbered"][qc][k] += 1
                hc = "fight" if a > e else "avoid" if a < e else "unsure"
                if hc != qc:
                    disagree[(qc, hc)][k] += 1
    # Does the estimate beat counting heads? Head-count rule: fight when we outnumber them, avoid
    # when they outnumber us, unsure at even numbers. Compared where the two disagree.
    print("\nquick version vs counting heads, where they disagree (outcome: we lost fewer / same / more)")
    for (qc, hc), (b, e, w) in sorted(disagree.items()):
        n = b + e + w
        print(f"  estimate {qc:6} / heads {hc:6}: {n:4}   fewer {100*b//n:3}%  same {100*e//n:3}%  more {100*w//n:3}%")
    compact("quick version by opponent", by_opp)
    compact("by our build", by_ours)
    compact("by fight size", by_size)
    compact("by numbers", by_odds)

if __name__ == "__main__":
    args = sys.argv[1:]
    by_opponent = "--by-opponent" in args
    args = [a for a in args if a != "--by-opponent"] or ["."]
    main(args)
    if by_opponent:
        breakdowns(args)
