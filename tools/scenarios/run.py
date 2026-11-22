"""Play the scenarios in tests/scenarios through the game, and compare each with its golden.

Usage, from the repo root:
    python tools/scenarios/run.py                     check every scenario against its golden
    python tools/scenarios/run.py --only NAME ...     just these
    python tools/scenarios/run.py --save [--only ...] write the goldens from what the game does now
    python tools/scenarios/run.py --diff              print each difference (the first 40 lines of a diff)
    python tools/scenarios/run.py --sanitize ...      build with AddressSanitizer and UBSan, so a memory error
                                                      stops the game and says where (the goldens are the same)

A scenario is tests/scenarios/NAME.scn (the format is in tools/scenarios/scenario.c), and its golden is NAME.golden:
the line "2.4b6-fixed", then the driver's output. Needs Docker: the game and the driver are built in one gcc:15
container (tools/scenarios/play.sh), and each scenario runs from a fresh copy of the area and player directories.
A scenario passes when the driver exits 0, or with the status its @exit line names, and its output matches.
Exit status: 0 if every scenario matches its golden (or was saved), 1 otherwise. Stdlib only.
"""
import argparse
import difflib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCENARIOS = ROOT / "tests" / "scenarios"
HEADER = "2.4b6-fixed\n"
IMAGE = "gcc:15"
SANITIZE = "-fsanitize=address,undefined -fno-omit-frame-pointer"


def play(names, work, extra=""):
    out = work / "out"
    out.mkdir()
    tree = work / "tree"
    for d in ("src", "area", "tests"):
        shutil.copytree(ROOT / d, tree / d, ignore=shutil.ignore_patterns("*.o", "rom", "scenario"))
    shutil.copytree(ROOT / "tools" / "scenarios", tree / "tools" / "scenarios")
    mount = lambda src, dst, ro: f"type=bind,source={src},target={dst}" + (",readonly" if ro else "")
    cmd = ["docker", "run", "--rm", "-e", "SCENARIOS=" + " ".join(names), "-e", f"EXTRA={extra}",
           "--mount", mount(tree, "/in", True), "--mount", mount(out, "/out", False),
           IMAGE, "bash", "/in/tools/scenarios/play.sh"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    log = out / "build.log"
    if r.returncode or not log.exists() or "build exit 0" not in log.read_text(errors="replace"):
        tail = log.read_text(errors="replace").splitlines()[-15:] if log.exists() else r.stderr.splitlines()[-15:]
        sys.exit("the build failed:\n  " + "\n  ".join(tail))
    return out


def expected_exit(name):
    """The status the driver should exit with: 0, or the scenario's @exit N (a game that stops at boot)."""
    for line in (SCENARIOS / f"{name}.scn").read_text(encoding="latin-1").splitlines():
        if line.startswith("@exit "):
            return line.split()[1]
    return "0"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--only", nargs="+", metavar="NAME")
    ap.add_argument("--save", action="store_true", help="write the goldens instead of checking them")
    ap.add_argument("--diff", action="store_true", help="print the start of each difference")
    ap.add_argument("--sanitize", action="store_true", help="build with AddressSanitizer and UBSan")
    args = ap.parse_args()
    names = args.only or sorted(p.stem for p in SCENARIOS.glob("*.scn"))
    missing = [n for n in names if not (SCENARIOS / f"{n}.scn").exists()]
    if missing:
        sys.exit("no such scenario: " + ", ".join(missing))

    work = Path(tempfile.mkdtemp(prefix="scenarios-"))
    try:
        out = play(names, work, SANITIZE if args.sanitize else "")
        failed = 0
        for n in names:
            code = (out / f"{n}.exit").read_text().strip() if (out / f"{n}.exit").exists() else "?"
            got = HEADER + (out / f"{n}.out").read_text(encoding="latin-1") if (out / f"{n}.out").exists() else None
            golden = SCENARIOS / f"{n}.golden"
            if code != expected_exit(n) or got is None:
                err = (out / f"{n}.err").read_text(errors="replace").strip().splitlines() if (out / f"{n}.err").exists() else []
                report = [l for l in err if "ERROR: AddressSanitizer" in l or "runtime error:" in l]
                want_code = expected_exit(n)
                print(f"ERROR     {n}: the driver exited {code}"
                      f"{', not ' + want_code if want_code != '0' else ''}: {(report or err or [''])[-1]}")
                if args.diff:
                    for l in [l for l in err if l.lstrip().startswith("#")][:8]:
                        print("    " + l.strip()[:200])
                failed += 1
            elif args.save:
                old = golden.read_text(encoding="latin-1") if golden.exists() else None
                golden.write_bytes(got.encode("latin-1"))
                print(f"{'SAVED' if old is None else 'SAME' if old == got else 'RESAVED':9} {n}")
            elif not golden.exists():
                print(f"NO GOLDEN {n}")
                failed += 1
            else:
                want = golden.read_text(encoding="latin-1")
                if want == got:
                    print(f"ok        {n}")
                else:
                    failed += 1
                    print(f"DIFFERS   {n}")
                    if args.diff:
                        d = difflib.unified_diff(want.splitlines(), got.splitlines(), "golden", "now", lineterm="", n=1)
                        for line in list(d)[:40]:
                            print("    " + line[:200])
        print(f"\n{len(names) - failed} of {len(names)} {'saved' if args.save else 'match'}")
        sys.exit(1 if failed else 0)
    finally:
        shutil.rmtree(work, ignore_errors=True)


main()
