#!/usr/bin/env python3
"""
Run each test listed in a text file (one test name per line) under -O2 and
record branch-prediction stats emitted by the simulator (BP_TOTAL,
BP_MISPREDICTS, BP_ACCURACY).

Output: benchmarking/results/branch_accuracy.csv

Usage:
    python3 benchmarking/branch_accuracy.py
    python3 benchmarking/branch_accuracy.py --tests-file benchmarking/tests.txt
    python3 benchmarking/branch_accuracy.py --out benchmarking/results/bp.csv

This script does NOT modify config.h or rebuild — it just runs the existing
processor binary, so make sure to `make` first.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO = SCRIPT_DIR.parent
PROCESSOR = REPO / "processor"
TEST_DIR = REPO / "test_data_pipeline"
RESULTS_DIR = SCRIPT_DIR / "results"
DEFAULT_TESTS_FILE = SCRIPT_DIR / "tests.txt"
DEFAULT_OUT = RESULTS_DIR / "branch_accuracy.csv"


def run(cmd, check=True):
    return subprocess.run(cmd, cwd=REPO, check=check,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True)


def compile_asm(test_name: str) -> Path:
    target = TEST_DIR / test_name
    asm_files = sorted(target.glob("*.s"))
    if not asm_files:
        raise FileNotFoundError(f"No .s file in {target}")
    out = target / f"{test_name}.out"
    run(["mipsel-linux-gnu-gcc", "-mips32", str(asm_files[0]),
         "-nostartfiles", "-Ttext=0", "-o", str(out)])
    return out


BP_TOTAL_RE = re.compile(r"^BP_TOTAL\s+(\d+)", re.MULTILINE)
BP_MIS_RE = re.compile(r"^BP_MISPREDICTS\s+(\d+)", re.MULTILINE)
BP_ACC_RE = re.compile(r"^BP_ACCURACY\s+([\d.]+)%", re.MULTILINE)


def extract_bp(out: str):
    t = BP_TOTAL_RE.search(out)
    m = BP_MIS_RE.search(out)
    a = BP_ACC_RE.search(out)
    if not (t and m and a):
        return None
    return {
        "total": int(t.group(1)),
        "mispredicts": int(m.group(1)),
        "accuracy_pct": float(a.group(1)),
    }


def load_tests(tests_file: Path):
    if not tests_file.exists():
        sys.exit(f"tests file not found: {tests_file}")
    tests = []
    for line in tests_file.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        tests.append(line)
    if not tests:
        sys.exit(f"no tests listed in {tests_file}")
    return tests


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tests-file", default=str(DEFAULT_TESTS_FILE),
                    help="Text file with one test name per line")
    ap.add_argument("--out", default=str(DEFAULT_OUT),
                    help="Output CSV path")
    args = ap.parse_args()

    if not PROCESSOR.exists():
        sys.exit(f"processor binary not found at {PROCESSOR} — run `make` first")

    tests = load_tests(Path(args.tests_file))
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    rows = []
    for test in tests:
        print(f"[{test}]", end=" ", flush=True)
        try:
            exe = compile_asm(test)
        except Exception as e:
            print(f"COMPILE ERR: {e}")
            rows.append({"test": test, "total_branches": "",
                         "mispredicts": "", "accuracy_pct": "",
                         "status": "COMPILE_ERR"})
            continue

        res = run([str(PROCESSOR), f"--bmk={exe}", "-O2"], check=False)
        bp = extract_bp(res.stdout)
        if bp is None:
            print("NO BP STATS")
            rows.append({"test": test, "total_branches": "",
                         "mispredicts": "", "accuracy_pct": "",
                         "status": "NO_BP_STATS"})
            continue

        print(f"total={bp['total']} mis={bp['mispredicts']} "
              f"acc={bp['accuracy_pct']:.4f}%")
        rows.append({
            "test": test,
            "total_branches": bp["total"],
            "mispredicts": bp["mispredicts"],
            "accuracy_pct": round(bp["accuracy_pct"], 4),
            "status": "OK",
        })

    cols = ["test", "total_branches", "mispredicts", "accuracy_pct", "status"]
    with out_path.open("w") as f:
        f.write(",".join(cols) + "\n")
        for r in rows:
            f.write(",".join(str(r[c]) for c in cols) + "\n")

    print(f"\nWrote {out_path}")


if __name__ == "__main__":
    main()
