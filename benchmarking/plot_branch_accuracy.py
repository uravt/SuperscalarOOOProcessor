#!/usr/bin/env python3
"""
Plot branch-prediction accuracy from benchmarking/results/branch_accuracy.csv.

Outputs (in benchmarking/graphs/):
  - branch_accuracy.png           bar chart of accuracy% per test
  - branch_mispredict_rate.png    bar chart of (total, mispredicts) per test

Usage:
    python3 benchmarking/plot_branch_accuracy.py
    python3 benchmarking/plot_branch_accuracy.py --csv path/to.csv
"""

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_CSV = SCRIPT_DIR / "results" / "branch_accuracy.csv"
GRAPHS_DIR = SCRIPT_DIR / "graphs"


def load_rows(csv_path: Path):
    rows = []
    with csv_path.open() as f:
        for r in csv.DictReader(f):
            if r.get("status") != "OK":
                continue
            try:
                r["total_branches"] = int(r["total_branches"])
                r["mispredicts"] = int(r["mispredicts"])
                r["accuracy_pct"] = float(r["accuracy_pct"])
            except (TypeError, ValueError):
                continue
            rows.append(r)
    return rows


def plot_accuracy(rows, out_dir: Path):
    rows = sorted(rows, key=lambda r: r["accuracy_pct"], reverse=True)
    tests = [r["test"] for r in rows]
    acc = [r["accuracy_pct"] for r in rows]

    fig, ax = plt.subplots(figsize=(max(8, 1.1 * len(tests)), 5))
    x = np.arange(len(tests))
    cmap = plt.get_cmap("RdYlGn")
    colors = [cmap(a / 100.0) for a in acc]
    bars = ax.bar(x, acc, color=colors, edgecolor="black")

    ax.set_xticks(x)
    ax.set_xticklabels(tests, rotation=30, ha="right")
    ax.set_ylim(0, 105)
    ax.set_ylabel("Branch Prediction Accuracy (%)")
    ax.set_title("Branch Prediction Accuracy by Test (-O2) (Bimodal)")
    ax.axhline(50, color="gray", linewidth=0.8, linestyle="--", alpha=0.5)
    ax.grid(axis="y", linestyle="--", alpha=0.4)

    for bar, a, r in zip(bars, acc, rows):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 1,
                f"{a:.1f}%\n({r['total_branches'] - r['mispredicts']}/{r['total_branches']})",
                ha="center", va="bottom", fontsize=8)

    fig.tight_layout()
    out = out_dir / "branch_accuracy.png"
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"wrote {out}")


def plot_counts(rows, out_dir: Path):
    rows = sorted(rows, key=lambda r: r["total_branches"], reverse=True)
    tests = [r["test"] for r in rows]
    correct = [r["total_branches"] - r["mispredicts"] for r in rows]
    mis = [r["mispredicts"] for r in rows]

    fig, ax = plt.subplots(figsize=(max(8, 1.1 * len(tests)), 5))
    x = np.arange(len(tests))
    ax.bar(x, correct, label="Correct", color="seagreen", edgecolor="black")
    ax.bar(x, mis, bottom=correct, label="Mispredicted",
           color="indianred", edgecolor="black")

    ax.set_xticks(x)
    ax.set_xticklabels(tests, rotation=30, ha="right")
    ax.set_ylabel("Branches Resolved")
    ax.set_title("Branch Outcomes by Test (-O2) (Bimodal)")
    ax.legend()
    ax.grid(axis="y", linestyle="--", alpha=0.4)

    totals = [c + m for c, m in zip(correct, mis)]
    for xi, total in zip(x, totals):
        ax.text(xi, total + max(totals) * 0.01, str(total),
                ha="center", va="bottom", fontsize=8)

    fig.tight_layout()
    out = out_dir / "branch_mispredict_rate.png"
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"wrote {out}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default=str(DEFAULT_CSV),
                    help="Branch accuracy CSV from branch_accuracy.py")
    args = ap.parse_args()

    csv_path = Path(args.csv)
    if not csv_path.exists():
        raise SystemExit(f"CSV not found: {csv_path}")

    rows = load_rows(csv_path)
    if not rows:
        raise SystemExit("No OK rows in CSV")

    GRAPHS_DIR.mkdir(parents=True, exist_ok=True)
    plot_accuracy(rows, GRAPHS_DIR)
    plot_counts(rows, GRAPHS_DIR)


if __name__ == "__main__":
    main()
