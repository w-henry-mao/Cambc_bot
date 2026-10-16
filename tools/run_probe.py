#!/usr/bin/env python3
"""Run a rule probe: run_probe.py <config_a> <config_b> [map] [rounds]. Prints both bots' P/ACT/PRED lines."""
import pathlib, shutil, subprocess, sys, zipfile
WS = pathlib.Path.home() / "cambc-ws"
PROBE = pathlib.Path(__file__).resolve().parent / "bots/probe"
def install(name, cfg):
    dst = WS / "bots" / name / "src"
    dst.mkdir(parents=True, exist_ok=True)
    shutil.copy(PROBE / "main.cpp", dst / "main.cpp")
    shutil.copy(PROBE / "configs" / f"{cfg}.h", dst / "probe_config.h")
a, b = sys.argv[1], sys.argv[2]
mp = sys.argv[3] if len(sys.argv) > 3 else "packs"
rounds = sys.argv[4] if len(sys.argv) > 4 else "120"
install("probe-a", a); install("probe-b", b)
out = WS / "replays" / "probe"
out.mkdir(parents=True, exist_ok=True)
before = set(out.glob("*.repl26a"))
r = subprocess.run([str(WS / ".venv/bin/cambc"), "run", "probe-a", "probe-b", "--map", mp, "--rounds", rounds, "-o", str(out)],
                   cwd=WS, capture_output=True, text=True)
new = sorted(set(out.glob("*.repl26a")) - before)
if not new:
    print(r.stdout[-2000:], r.stderr[-2000:]); sys.exit(1)
with zipfile.ZipFile(new[-1]) as z:
    for team in ("team-a", "team-b"):
        text = z.read(f"{team}.stdout").decode("utf-8", "replace")
        for line in text.splitlines():
            line = "".join(ch for ch in line if ch.isprintable())
            if line.startswith(("P ", "ACT", "PRED", "bot error")): print(team[-1].upper(), line)
