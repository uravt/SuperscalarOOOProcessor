#!/usr/bin/env python3
"""
Plot CPI vs pipeline width for each test in benchmarking/results/combined.csv.

For each test, produces a bar chart (x=PIPELINE_WIDTH, y=CPI) with the
speedup-vs-width-1 ratio annotated on top of each bar. Also writes one
combined grouped chart across all tests.

Output: benchmarking/graphs/<test>.png and benchmarking/graphs/all_tests.png

Usage:
    python3 benchmarking/plot_results.py
    python3 benchmarking/plot_results.py --csv benchmarking/results/combined.csv
"""

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_CSV = SCRIPT_DIR / "results" / "combined.csv"
GRAPHS_DIR = SCRIPT_DIR / "graphs"


def load_rows(csv_path: Path):
    rows_by_test = defaultdict(list)
    with csv_path.open() as f:
        reader = csv.DictReader(f)
        for r in reader:
            if r.get("status") != "OK":
                continue
            try:
                r["width"] = int(r["width"])
                r["cpi"] = float(r["cpi"])
                r["speedup_vs_w1"] = float(r["speedup_vs_w1"])
            except (TypeError, ValueError):
                continue
            rows_by_test[r["test"]].append(r)
    for t in rows_by_test:
        rows_by_test[t].sort(key=lambda x: x["width"])
    return rows_by_test


def plot_per_test(test: str, rows, out_dir: Path):
    widths = [r["width"] for r in rows]
    cpis = [r["cpi"] for r in rows]
    speedups = [r["speedup_vs_w1"] for r in rows]

    fig, ax = plt.subplots(figsize=(7, 4.5))
    x = np.arange(len(widths))
    bars = ax.bar(x, cpis, color="steelblue", edgecolor="black")

    ax.set_xticks(x)
    ax.set_xticklabels([str(w) for w in widths])
    ax.set_xlabel("Pipeline Width (= NUM_ALUS)")
    ax.set_ylabel("CPI (cycles / instruction)")
    ax.set_title(f"{test}: CPI vs Pipeline Width")
    ax.grid(axis="y", linestyle="--", alpha=0.4)

    ymax = max(cpis) if cpis else 1
    ax.set_ylim(0, ymax * 1.18)
    for bar, sp in zip(bars, speedups):
        ax.text(bar.get_x() + bar.get_width() / 2,
                bar.get_height() + ymax * 0.02,
                f"{sp:.2f}x",
                ha="center", va="bottom", fontsize=10, fontweight="bold")

    fig.tight_layout()
    out = out_dir / f"{test}.png"
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"  wrote {out}")


def plot_all_tests(rows_by_test, out_dir: Path):
    tests = sorted(rows_by_test.keys())
    if not tests:
        return
    widths = sorted({r["width"] for rows in rows_by_test.values() for r in rows})

    fig, ax = plt.subplots(figsize=(max(8, 1.4 * len(tests)), 5))
    x = np.arange(len(tests))
    bar_w = 0.8 / len(widths)

    cmap = plt.get_cmap("viridis")
    for i, w in enumerate(widths):
        cpis, sps = [], []
        for t in tests:
            row = next((r for r in rows_by_test[t] if r["width"] == w), None)
            cpis.append(row["cpi"] if row else 0)
            sps.append(row["speedup_vs_w1"] if row else None)
        offset = (i - (len(widths) - 1) / 2) * bar_w
        bars = ax.bar(x + offset, cpis, bar_w,
                      label=f"width={w}", color=cmap(i / max(1, len(widths) - 1)),
                      edgecolor="black", linewidth=0.5)
        ymax_local = max(cpis) if cpis else 1
        for bar, sp in zip(bars, sps):
            if sp is None or bar.get_height() == 0:
                continue
            ax.text(bar.get_x() + bar.get_width() / 2,
                    bar.get_height() + ymax_local * 0.01,
                    f"{sp:.2f}x",
                    ha="center", va="bottom", fontsize=7, rotation=90)

    ax.set_xticks(x)
    ax.set_xticklabels(tests, rotation=30, ha="right")
    ax.set_ylabel("CPI")
    ax.set_title("CPI vs Pipeline Width (speedup vs w=1 above each bar)")
    ax.legend()
    ax.grid(axis="y", linestyle="--", alpha=0.4)

    fig.tight_layout()
    out = out_dir / "all_tests.png"
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"  wrote {out}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default=str(DEFAULT_CSV),
                    help="Combined CSV from sweep_pipeline_width.py")
    args = ap.parse_args()

    csv_path = Path(args.csv)
    if not csv_path.exists():
        raise SystemExit(f"CSV not found: {csv_path}")

    GRAPHS_DIR.mkdir(parents=True, exist_ok=True)
    rows_by_test = load_rows(csv_path)
    if not rows_by_test:
        raise SystemExit("No OK rows in CSV")

    print(f"Plotting {len(rows_by_test)} tests -> {GRAPHS_DIR}")
    for test, rows in sorted(rows_by_test.items()):
        plot_per_test(test, rows, GRAPHS_DIR)
    plot_all_tests(rows_by_test, GRAPHS_DIR)


if __name__ == "__main__":
    main()
