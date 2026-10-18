#!/usr/bin/env python3
"""Where champion time and gold go, from STATE_TRACE lines (builds with STATE_TRACE=true).

  trace_report.py <report> <replays-dir> <variant>

Runs are directories r_<variant>__<opponent>__<side>__<map> holding one .repl26a bundle; our
stdout is team-<side>.stdout. Line format (bot.cpp):
  ST round champ state x y gems hp maxhp seen nearest target [dist respawn] [SW old dist respawn
  waiting] A<action> G<gold> D<dist,can_move,live>
state: 0 recall, 1 attack, 2 tribute, 3 farm, 4 none. Farm actions (states/farm.h): H hit a camp,
h camp in range but no attack, C capturing, S stepping off, W waiting for a respawn, A approaching
a live group, E an earlier return.

Reports:
  states    share of rounds per champion: state, and in the farm state at camp / walking
  actions   farm actions per champion, overall and within 3 tiles of the target
  trips     farm trips (runs of one target): how they ended, walking per trip
  switches  farm-to-farm target changes by why the old target was dropped
  gold      gold gained per game by what the champion was doing
"""
import collections, pathlib, statistics, sys, zipfile

STATES = ["recall", "attack", "tribute", "farm", "none"]


def runs(root, variant):
    """Yields each run's lines split into tokens (ST lines only)."""
    for run in sorted(pathlib.Path(root).glob(f"r_{variant}__*")):
        side = run.name.split("__")[2]
        bundles = list(run.glob("*.repl26a"))
        if not bundles:
            continue
        text = zipfile.ZipFile(bundles[0]).read(f"team-{side}.stdout").replace(b"\0", b"").decode(errors="replace")
        yield [p for p in (line.split() for line in text.splitlines()) if len(p) >= 4 and p[0] == "ST"]


def action(p):
    return next((t[1:] for t in p if t.startswith("A") and len(t) == 2), None)


def gold(p):
    return next((int(t[1:]) for t in p if t.startswith("G") and t[1:].isdigit()), None)


def target_dist(p):
    """(target, distance, respawn) when the line has them."""
    if len(p) > 13 and p[12].isdigit() and p[13].isdigit():
        return int(p[11]), int(p[12]), int(p[13])
    return None


def report_states(root, variant):
    per = collections.defaultdict(collections.Counter)
    for lines in runs(root, variant):
        for p in lines:
            st = int(p[3])
            if len(p) <= 4:
                c = "dead"
            elif st == 3:
                t = target_dist(p)
                c = "farm:no target" if not t else "farm:waiting" if t[2] > 0 and t[1] <= 3 else "farm:at camp" if t[1] <= 3 else "farm:walking"
            else:
                c = STATES[st]
            per[p[2]][c] += 1
    cats = ["recall", "attack", "tribute", "farm:at camp", "farm:waiting", "farm:walking", "farm:no target"]
    print(f"{'champ':9}" + "".join(f"{c:>15}" for c in cats))
    for ch in sorted(per):
        tot = sum(per[ch].values())
        print(f"{ch:9}" + "".join(f"{100 * per[ch][c] / tot:14.0f}%" for c in cats))


def report_actions(root, variant):
    names = {"H": "hit", "h": "no attack", "A": "approach", "W": "wait respawn", "C": "capturing", "S": "step off", "E": "early return"}
    cols = list(names)
    per = collections.defaultdict(collections.Counter)
    near = collections.defaultdict(collections.Counter)
    for lines in runs(root, variant):
        for p in lines:
            ch, act = p[2], action(p)
            per[ch]["all"] += 1
            if p[3] != "3" or act in (None, "-"):
                continue
            per[ch][act] += 1
            t = target_dist(p)
            if t and t[1] <= 3:
                near[ch][act] += 1
    print("% of all rounds")
    print(f"{'champ':9}" + "".join(f"{names[c]:>14}" for c in cols))
    for ch in sorted(per):
        print(f"{ch:9}" + "".join(f"{100 * per[ch][c] / per[ch]['all']:13.1f}%" for c in cols))
    print("\nwithin 3 tiles of the target, % of those rounds")
    for ch in sorted(near):
        tot = sum(near[ch].values())
        print(f"{ch:9}" + "".join(f"{100 * near[ch][c] / tot:13.1f}%" for c in cols))


def report_trips(root, variant):
    trips = []   # (reached, walk rounds, start distance, how it ended)
    for lines in runs(root, variant):
        cur = {}
        for p in lines:
            ch, st = p[2], int(p[3])
            t = target_dist(p)
            if st != 3 or not t:
                if ch in cur:
                    trip = cur.pop(ch)
                    trips.append((trip["arr"] is not None, trip["arr"] if trip["arr"] is not None else trip["n"], trip["d0"], "dead" if len(p) <= 4 else STATES[st]))
                continue
            tgt, dist, _ = t
            trip = cur.get(ch)
            if trip and trip["tgt"] != tgt:
                trips.append((trip["arr"] is not None, trip["arr"] if trip["arr"] is not None else trip["n"], trip["d0"], "switch"))
                trip = None
            if not trip:
                trip = cur[ch] = {"tgt": tgt, "n": 0, "arr": None, "d0": dist}
            if dist <= 3 and trip["arr"] is None:
                trip["arr"] = trip["n"]
            trip["n"] += 1
    if not trips:
        return print("no trips")
    ends = collections.Counter((("reached" if r else "not reached"), e) for r, _, _, e in trips)
    print(f"trips {len(trips)}")
    for (r, e), v in ends.most_common():
        print(f"  {r:12} then {e:9} {v:6} {100 * v / len(trips):3.0f}%")
    walk = [w for _, w, _, _ in trips]
    lost = sum(w for r, w, _, _ in trips if not r)
    print(f"walk rounds per trip: mean {statistics.mean(walk):.1f}; on trips that never arrived {100 * lost / max(1, sum(walk)):.0f}%")


def report_switches(root, variant):
    why = collections.Counter()
    farm_rounds = 0
    for lines in runs(root, variant):
        prev = {}
        for p in lines:
            ch, st = p[2], int(p[3])
            farm_rounds += st == 3
            if st == 3 and prev.get(ch) == 3 and "SW" in p:
                i = p.index("SW")
                dist, resp, waiting = int(p[i + 2]), int(p[i + 3]), int(p[i + 4])
                k = ("reward waiting" if waiting else "respawning" if resp > 0 else "still up") + (" (arrived)" if dist <= 3 else " (en route)")
                why[k] += 1
            prev[ch] = st
    tot = sum(why.values())
    print(f"farm-to-farm switches {tot}, {100 * tot / max(1, farm_rounds):.1f} per 100 farm rounds; old target:")
    for k, v in why.most_common():
        print(f"  {k:28} {v:6} {100 * v / tot:3.0f}%")


def report_gold(root, variant):
    gain = collections.defaultdict(collections.Counter)
    games = 0
    for lines in runs(root, variant):
        games += 1
        last = {}
        for p in lines:
            g = gold(p)
            if g is None:
                continue
            ch, st = p[2], int(p[3])
            if ch in last:
                g0, st0, act0 = last[ch]
                if g > g0:
                    gain[ch][f"farm:{act0}" if st0 == 3 else STATES[st0]] += g - g0
            last[ch] = (g, st, action(p) or "-")
    print(f"games {games}; gold gained per game by what the champion was doing the turn before")
    print("(spending between turns hides gains: Tavi, who shops anywhere, shows little)")
    for ch in sorted(gain):
        top = ", ".join(f"{k} {v / games:.0f}" for k, v in gain[ch].most_common(5))
        print(f"{ch:9} {sum(gain[ch].values()) / games:6.0f} | {top}")


if __name__ == "__main__":
    reports = {"states": report_states, "actions": report_actions, "trips": report_trips,
               "switches": report_switches, "gold": report_gold}
    if len(sys.argv) != 4 or sys.argv[1] not in reports:
        sys.exit(__doc__)
    reports[sys.argv[1]](sys.argv[2], sys.argv[3])
