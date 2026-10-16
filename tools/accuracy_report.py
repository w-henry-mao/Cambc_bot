#!/usr/bin/env python3
"""In-match accuracy of the fight calculator.

  accuracy_report.py <replay-dir> [<replay-dir> ...]

Reads every .repl26a bundle; for each, the side whose bot logged FIGHT_PRED lines is ours
(both sides in self-play). Each FIGHT_PRED is paired with the next FIGHT_END of the same
champion. A fight counts when it finished (someone died), stayed clean (no third champion or
awake camp) and our bot kept fighting (policy_ok). Against opponents that are not maximally
aggressive the real result must never be worse than the full version's worst case.
"""
import json, pathlib, re, statistics, sys, zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
# Per champion type: biggest offensive spell multiplier and whether any ability can stun.
DESIGNS = {}
for _l in (ROOT / "tools/data/designs.jsonl").read_text().splitlines():
    _d = json.loads(_l)
    _off = [x for x in _d["slots"] if x["kind"] in ("damage", "aoe_damage", "cc")]
    DESIGNS[_d["type"]] = (max([x["modifier"] for x in _off], default=0.0),
                           any(x["duration"] > 0 and x["kind"] in ("cc", "aoe_damage") for x in _off)
                           or any(x["kind"] == "attack_reset" and x["enhanced"] for x in _d["slots"]))

def kv(line, key):
    m = re.search(rf"\b{key}=([-\w.]+)", line)
    return m.group(1) if m else None

def fights(path):
    with zipfile.ZipFile(path) as z:
        for team in ("team-a", "team-b"):
            text = z.read(f"{team}.stdout").decode("utf-8", "replace")
            text = "".join(ch if ch.isprintable() or ch == "\n" else " " for ch in text)
            open_pred, third_party, last_hp, last_gap_max = {}, {}, {}, {}
            for line in text.splitlines():
                if "FIGHT_PRED" in line:
                    me = kv(line, "me")
                    open_pred[me] = line[line.index("FIGHT_PRED"):]
                    third_party[me] = False
                    last_hp[me] = float(kv(line, "our_hp"))
                elif "FIGHT_TURN" in line:
                    me = kv(line, "me")
                    if me not in open_pred: continue
                    mu, me_ = re.search(r"S_us=(\S+)", line), re.search(r"S_en=(\S+)", line)
                    if not mu or not me_: continue   # line cut off
                    us, en = mu.group(1).split(","), me_.group(1).split(",")
                    if len(us) < 5 or len(en) < 5: continue
                    hp = float(us[1])
                    # Most this enemy can deal between two of our turns: two turns, each two basic
                    # hits plus its strongest spell. More than that, or a stun from an enemy that
                    # cannot stun, means someone else hit us.
                    en_str, en_focus = float(en[3]), float(en[4])
                    max_mod, can_stun = DESIGNS.get(int(en[0]), (2.5, True))
                    last_gap_max[me] = 2 * (2 * en_str + max_mod * en_focus) + 1
                    if last_hp[me] - hp > last_gap_max[me]:
                        third_party[me] = True
                    if int(us[9]) > 0 and not can_stun:
                        third_party[me] = True
                    last_hp[me] = hp
                elif "FIGHT_END" in line:
                    me = kv(line, "me")
                    if me in open_pred:
                        end = line[line.index("FIGHT_END"):]
                        # The killing gap has no FIGHT_TURN line: check the final drop too.
                        if kv(end, "how") == "we_died" and last_hp.get(me, 0) > last_gap_max.get(me, 1e9):
                            third_party[me] = True
                        if third_party.pop(me, False): end += " third_party=1"
                        yield path.name, team, open_pred.pop(me), end

def main(dirs):
    total = finished = counted = violations = win_ok = predicted_win_lost = predicted_loss_won = 0
    errors, skipped = [], {"separated": 0, "unclean": 0, "policy": 0, "unresolved": 0}
    bad = []
    for d in dirs:
        for f in sorted(pathlib.Path(d).glob("*.repl26a")):
            for name, team, pred, end in fights(f):
                total += 1
                how = kv(end, "how")
                if how == "separated": skipped["separated"] += 1; continue
                finished += 1
                if kv(pred, "clean") != "1" or kv(end, "clean") != "1" or kv(end, "third_party") == "1":
                    skipped["unclean"] += 1; continue
                if kv(end, "policy_ok") != "1": skipped["policy"] += 1; continue
                if kv(pred, "full_resolved") != "1": skipped["unresolved"] += 1; continue
                counted += 1
                actual_win = how == "enemy_died"
                actual_hp = 0.0 if how == "we_died" else float(kv(end, "our_hp"))
                worst = float(kv(pred, "full_hp"))
                predicted_win = kv(pred, "full_win") == "1"
                win_ok += predicted_win == actual_win
                predicted_win_lost += predicted_win and not actual_win
                predicted_loss_won += actual_win and not predicted_win
                errors.append(actual_hp - worst)
                if worst > actual_hp + 0.5:
                    violations += 1
                    bad.append(f"  {name} {team}: {kv(pred,'me')} vs {kv(pred,'enemy')} r={kv(pred,'r')} "
                               f"worst={worst:g} actual={actual_hp:g} ({how})")
    print(f"fights logged: {total}; finished: {finished}; counted (clean, finished, fought, resolved): {counted}")
    print(f"skipped: {skipped}")
    print(f"bound violations (actual worse than worst case): {violations}")
    if counted:
        print(f"worst case said win, we lost (must be 0): {predicted_win_lost}")
        print(f"worst case said loss, we won (opponent weaker than worst case): {predicted_loss_won}")
        print(f"same outcome as worst case: {win_ok}/{counted} ({100 * win_ok / counted:.0f}%)")
        print(f"actual - worst-case health: median {statistics.median(errors):.0f}, "
              f"mean {statistics.mean(errors):.0f}, max {max(errors):.0f}")
    for b in bad[:20]: print(b)
    return 1 if violations or predicted_win_lost else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
