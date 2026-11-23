"""Build this tree with several compilers, play the same session on each, and compare them with a reference build.

Usage, from the repo root:   python tools/buildcheck/run.py [--only NAME ...] [--keep]

Needs Docker. Each build runs in a fresh container (tools/buildcheck/check.sh): it builds, boots the game on a
port, and plays a scripted session as a new character (creation, then look, score, help rom, who, inventory,
equipment and quit). What's compared with the reference:
  - the boot log, without its timestamps
  - the session, byte for byte
  - the saved player file, without Id and LogO (the times it was made and last left) and Plyd (the seconds played,
    which a slow container can push by one)

The reference is stock ROM 2.4b6 (the stock-2.4b6 tag) with only this tree's src/tables.h, built with gcc 4.9 and
stock's own Makefile.linux. Stock as released doesn't build on any compiler there's an image for: gcc 4.9 already
rejects its tables.h. A build change should come out the same as the reference; a bug fix changes it on purpose,
and the differences are printed for its PR. Exit status: 0 if every build matches the reference, 1 otherwise.
Stdlib only.
"""
import argparse
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from io import BytesIO
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
CHECK = Path(__file__).resolve().parent / "check.sh"
CLANG = "apt-get -qq update >/dev/null && apt-get -qq install -y clang >/dev/null 2>&1 && "
# name, tree, image, compiler, makefile, setup
BUILDS = [
    ("reference", "reference", "gcc:4.9", "gcc", "Makefile.linux", ""),
    ("gcc-4.9", "tree", "gcc:4.9", "gcc", "Makefile", ""),
    ("gcc-15", "tree", "gcc:15", "gcc", "Makefile", ""),
    ("clang", "tree", "gcc:15", "clang", "Makefile", CLANG),
]
STAMP = re.compile(rb"^[A-Z][a-z]{2} [A-Z][a-z]{2} +\d+ \d\d:\d\d:\d\d \d{4} :: ", re.M)
TIMES = re.compile(rb"^(Id|LogO|Plyd) +\d+\r?\n", re.M)


def trees(work):
    """The reference (stock plus this tree's tables.h) and this tree, each with src/ and area/, under work."""
    ref = work / "reference"
    tar = subprocess.run(["git", "-C", str(ROOT), "archive", "stock-2.4b6", "src", "area"],
                         capture_output=True, check=True).stdout
    with tarfile.open(fileobj=BytesIO(tar)) as t:
        t.extractall(ref, filter="data")
    shutil.copy2(ROOT / "src" / "tables.h", ref / "src" / "tables.h")
    tree = work / "tree"
    for d in ("src", "area"):
        shutil.copytree(ROOT / d, tree / d, ignore=shutil.ignore_patterns("*.o", "rom"))


def run(work, out, name, tree, image, cc, makefile, setup):
    mount = lambda src, dst, ro: f"type=bind,source={src},target={dst}" + (",readonly" if ro else "")
    cmd = ["docker", "run", "--rm", "-e", f"TAG={name}", "-e", f"CC={cc}", "-e", f"MAKEFILE={makefile}",
           "--mount", mount(work / tree, "/in", True), "--mount", mount(out, "/out", False),
           "--mount", mount(CHECK, "/check.sh", True), image, "bash", "-c", setup + "bash /check.sh"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    result = out / name / "result.txt"
    status = result.read_text().strip() if result.exists() else (r.stderr.strip().splitlines() or ["no result"])[-1]
    return status


def read(path, drop=None):
    if not path.exists():
        return None
    data = path.read_bytes()
    return drop.sub(b"", data) if drop else data


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--only", nargs="+", help="builds to run besides the reference: "
                    + ", ".join(b[0] for b in BUILDS[1:]))
    ap.add_argument("--keep", action="store_true", help="keep the work directory and print where it is")
    args = ap.parse_args()
    builds = [b for b in BUILDS if b[0] == "reference" or not args.only or b[0] in args.only]

    work = Path(tempfile.mkdtemp(prefix="buildcheck-"))
    out = work / "out"
    out.mkdir()
    trees(work)
    failed = False
    rows = []
    for b in builds:
        print(f"{b[0]}: {b[2]}, {b[3]}, {b[4]} ...", flush=True)
        status = run(work, out, *b)
        d = out / b[0]
        compiler = (d / "compiler.txt").read_text().strip() if (d / "compiler.txt").exists() else "?"
        log = (d / "build.log").read_text(errors="replace") if (d / "build.log").exists() else ""
        rows.append((b[0], compiler, status, log.count(" warning: ")))
        if status != "ok":
            failed = True
            print(f"  failed: {status}")
    print()
    print(f"{'build':10} {'compiler':38} {'result':8} {'warnings':>8}  boot log  session  player file")
    ref = out / "reference"
    base = (read(ref / "boot.log", STAMP), read(ref / "session.txt"), read(ref / "player.txt", TIMES))
    for name, compiler, status, warnings in rows:
        d = out / name
        got = (read(d / "boot.log", STAMP), read(d / "session.txt"), read(d / "player.txt", TIMES))
        if name == "reference":
            same = [f"{len(x.splitlines()) if x is not None else '-'} lines" if i == 0 else
                    f"{len(x) if x is not None else '-'} bytes" for i, x in enumerate(got)]
        else:
            same = ["same" if g is not None and g == r else "DIFFERS" for g, r in zip(got, base)]
            failed |= "DIFFERS" in same
        print(f"{name:10} {compiler[:38]:38} {status:8} {warnings:8}  {same[0]:9} {same[1]:8} {same[2]}")
    if args.keep:
        print(f"\nkept: {work}")
    else:
        shutil.rmtree(work, ignore_errors=True)
    sys.exit(1 if failed else 0)


main()
