#!/usr/bin/env python3
"""Copy the bot into the workspace as a test variant with compile-time overrides.

  make_test_variant.py <variant-name> [NAME=VALUE ...]

Creates ~/cambc-ws/bots/<variant-name>/ (src/ copied from this repo) with
src/constants/test_overrides.h holding `#define NAME VALUE` lines. COMBAT_TRACE=true is
always set. Constants that support overrides are wrapped in #ifndef in their headers.
"""
import pathlib, shutil, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
WS = pathlib.Path.home() / "cambc-ws" / "bots"
name, overrides = sys.argv[1], dict(a.split("=", 1) for a in sys.argv[2:])
overrides.setdefault("COMBAT_TRACE", "true")
dst = WS / name
# Never overwrite a real checkout (this repo, a git worktree, or any other git checkout).
if dst.exists() and (dst.resolve() == ROOT.resolve() or (dst / ".git").exists()):
    sys.exit(f"refusing to overwrite {dst}: it is a git checkout, not a test variant")
if (dst / "src").exists():
    shutil.rmtree(dst / "src")
shutil.copytree(ROOT / "src", dst / "src")
(dst / "src/constants/test_overrides.h").write_text(
    "#pragma once\n" + "".join(f"#define {k} {v}\n" for k, v in overrides.items()))
print(f"variant {name}: {overrides}")
