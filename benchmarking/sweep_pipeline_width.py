#!/usr/bin/env python3
"""
Sweep PIPELINE_WIDTH (and NUM_ALUS) over {1,2,4,8} for each test listed in
tests.txt, collect cycle counts, compute CPI and speedup vs width=1, and
verify register state against the single-cycle reference.

Output: per-test CSV/JSON plus a combined CSV in benchmarking/results/.

Usage:
    python3 benchmarking/sweep_pipeline_width.py
    python3 benchmarking/sweep_pipeline_width.py --tests-file benchmarking/tests.txt
    python3 benchmarking/sweep_pipeline_width.py --widths 1,2,4,8
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent          # .../benchmarking
REPO = SCRIPT_DIR.parent                              # repo root
CONFIG_H = REPO / "config.h"
PROCESSOR = REPO / "processor"
TEST_DIR = REPO / "test_data_pipeline"
RESULTS_DIR = SCRIPT_DIR / "results"
DEFAULT_TESTS_FILE = SCRIPT_DIR / "tests.txt"


def run(cmd, cwd=REPO, check=True):
    return subprocess.run(cmd, cwd=cwd, check=check,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True)


def patch_config(width: int):
    text = CONFIG_H.read_text()
    new = re.sub(r"(constexpr\s+int\s+PIPELINE_WIDTH\s*=\s*)\d+",
                 rf"\g<1>{width}", text)
    new = re.sub(r"(constexpr\s+int\s+NUM_ALUS\s*=\s*)\d+",
                 rf"\g<1>{width}", new)
    CONFIG_H.write_text(new)


def rebuild():
    run(["make", "clean"])
    res = run(["make", "-j"], check=False)
    if res.returncode != 0:
        print(res.stdout)
        raise RuntimeError("Build failed")
    if not PROCESSOR.exists():
        raise RuntimeError("Build reported success but processor binary missing")


def compile_asm(test_name: str) -> Path:
    target = TEST_DIR / test_name
    asm_files = sorted(target.glob("*.s"))
    if not asm_files:
        raise FileNotFoundError(f"No .s file in {target}")
    asm = asm_files[0]
    out = target / f"{test_name}.out"
    run(["mipsel-linux-gnu-gcc", "-mips32", str(asm),
         "-nostartfiles", "-Ttext=0", "-o", str(out)])
    return out


CYCLE_RE = re.compile(r"^CYCLE\s+(\d+)", re.MULTILINE)
REG_RE = re.compile(r"^R\[.*", re.MULTILINE)


def extract(out: str):
    cycles = None
    m = list(CYCLE_RE.finditer(out))
    if m:
        cycles = int(m[-1].group(1))
    regs = REG_RE.findall(out)
    if len(regs) >= 32:
        regs = regs[-32:]
    return cycles, regs


def run_processor(exec_path: Path, opt: str):
    res = run([str(PROCESSOR), f"--bmk={exec_path}", opt], check=False)
    return res.stdout


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


def sweep_one_test(test_name: str, widths, results_dir: Path):
    print(f"\n===== {test_name} =====")
    exec_path = compile_asm(test_name)

    # Single-cycle reference run
    patch_config(widths[0])
    rebuild()
    sc_out = run_processor(exec_path, "-O0")
    sc_cycles, sc_regs = extract(sc_out)
    if sc_cycles is None:
        raise RuntimeError(f"[{test_name}] single-cycle run produced no CYCLE line")
    instructions = sc_cycles  # -O0: 1 instr/cycle

    rows = []
    baseline_cycles = None
    for w in widths:
        patch_config(w)
        rebuild()
        pipe_out = run_processor(exec_path, "-O2")
        cyc, regs = extract(pipe_out)
        if cyc is None or cyc <= 0:
            rows.append({
                "test": test_name, "width": w, "num_alus": w,
                "cycles": None, "instructions": instructions,
                "cpi": None, "ipc": None, "speedup_vs_w1": None,
                "regs_match_single_cycle": False, "status": "ERR/CRASH",
            })
            print(f"  [w={w}] CRASH")
            continue
        cpi = cyc / instructions
        ipc = instructions / cyc
        if baseline_cycles is None:
            baseline_cycles = cyc
        speedup = baseline_cycles / cyc
        regs_match = (regs == sc_regs) and len(regs) > 0
        rows.append({
            "test": test_name, "width": w, "num_alus": w,
            "cycles": cyc, "instructions": instructions,
            "cpi": round(cpi, 6), "ipc": round(ipc, 6),
            "speedup_vs_w1": round(speedup, 6),
            "regs_match_single_cycle": regs_match, "status": "OK",
        })
        print(f"  [w={w}] cycles={cyc} cpi={cpi:.4f} "
              f"speedup={speedup:.4f} regs_match={regs_match}")

    cols = ["test", "width", "num_alus", "cycles", "instructions", "cpi",
            "ipc", "speedup_vs_w1", "regs_match_single_cycle", "status"]
    csv_path = results_dir / f"{test_name}.csv"
    json_path = results_dir / f"{test_name}.json"
    with csv_path.open("w") as f:
        f.write(",".join(cols) + "\n")
        for r in rows:
            f.write(",".join(str(r[c]) for c in cols) + "\n")
    with json_path.open("w") as f:
        json.dump({
            "test": test_name,
            "single_cycle_cycles": sc_cycles,
            "instructions": instructions,
            "rows": rows,
        }, f, indent=2)
    return rows, cols


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tests-file", default=str(DEFAULT_TESTS_FILE),
                    help="Path to tests.txt (one test name per line)")
    ap.add_argument("--widths", default="1,2,4,8",
                    help="Comma-separated pipeline widths (default 1,2,4,8)")
    args = ap.parse_args()

    widths = [int(x) for x in args.widths.split(",") if x.strip()]
    if not widths:
        sys.exit("no widths")

    tests = load_tests(Path(args.tests_file))
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)

    backup = CONFIG_H.read_text()
    all_rows = []
    cols = None

    try:
        for test_name in tests:
            try:
                rows, cols = sweep_one_test(test_name, widths, RESULTS_DIR)
                all_rows.extend(rows)
            except Exception as e:
                print(f"  ERROR on {test_name}: {e}")

        if cols and all_rows:
            combined = RESULTS_DIR / "combined.csv"
            with combined.open("w") as f:
                f.write(",".join(cols) + "\n")
                for r in all_rows:
                    f.write(",".join(str(r[c]) for c in cols) + "\n")
            print(f"\nWrote {combined}")
    finally:
        CONFIG_H.write_text(backup)
        try:
            rebuild()
        except Exception as e:
            print(f"WARN: rebuild after restore failed: {e}")


if __name__ == "__main__":
    main()
