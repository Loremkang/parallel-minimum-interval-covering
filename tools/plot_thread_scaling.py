#!/usr/bin/env python3

import csv
import math
import sys
from pathlib import Path
from statistics import mean

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
if not 2 <= len(sys.argv) <= 3:
    print(f"Usage: {sys.argv[0]} [input.csv] [output-directory]")
    sys.exit(1)
INPUT_FILE = Path(sys.argv[1]).resolve()
PLOTS_DIR = Path(sys.argv[2]).resolve() if len(sys.argv) == 3 else PROJECT_ROOT / "plots"

if not INPUT_FILE.exists():
    print(f"Error: {INPUT_FILE} not found")
    print("Please run: tools/run_benchmarks.sh scaling")
    sys.exit(1)

required_columns = {
    "implementation",
    "n",
    "target_cover_size",
    "actual_cover_size",
    "threads",
    "time_ms",
    "throughput_M_per_sec",
}
data = []
with INPUT_FILE.open(newline="") as csv_file:
    reader = csv.DictReader(csv_file)
    if not required_columns.issubset(reader.fieldnames or []):
        print(f"Error: {INPUT_FILE} uses an obsolete or invalid CSV format")
        print("Please rerun: tools/run_benchmarks.sh scaling")
        sys.exit(1)
    for row in reader:
        data.append(
            {
                "implementation": row["implementation"],
                "n": int(row["n"]),
                "target_cover_size": int(row["target_cover_size"]),
                "actual_cover_size": int(row["actual_cover_size"]),
                "threads": int(row["threads"]),
                "time_ms": float(row["time_ms"]),
                "throughput_M_per_sec": float(row["throughput_M_per_sec"]),
            }
        )

if not data:
    print(f"Error: {INPUT_FILE} contains no measurements")
    sys.exit(1)


def select(implementation=None, n=None, target=None, threads=None):
    rows = data
    if implementation is not None:
        rows = [row for row in rows if row["implementation"] == implementation]
    if n is not None:
        rows = [row for row in rows if row["n"] == n]
    if target is not None:
        rows = [row for row in rows if row["target_cover_size"] == target]
    if threads is not None:
        rows = [row for row in rows if row["threads"] == threads]
    return rows


def average(field, implementation, n, target, threads=None):
    rows = select(implementation, n, target, threads)
    return mean(row[field] for row in rows) if rows else None


def make_subplots(title):
    columns = min(2, len(instances))
    rows = math.ceil(len(instances) / columns)
    figure, axes = plt.subplots(
        rows, columns, figsize=(7 * columns, 5 * rows), squeeze=False
    )
    figure.suptitle(title, fontsize=15, fontweight="bold")
    flat_axes = list(axes.flat)
    for axis in flat_axes[len(instances) :]:
        axis.set_visible(False)
    return figure, flat_axes


def save(figure, stem):
    figure.tight_layout(rect=(0, 0, 1, 0.96))
    figure.savefig(PLOTS_DIR / f"{stem}.png", dpi=300, bbox_inches="tight")
    figure.savefig(PLOTS_DIR / f"{stem}.pdf", bbox_inches="tight")
    plt.close(figure)


plt.rcParams["font.size"] = 11
plt.rcParams["lines.linewidth"] = 2
plt.rcParams["axes.grid"] = True
plt.rcParams["grid.alpha"] = 0.3
PLOTS_DIR.mkdir(exist_ok=True)

instances = sorted({(row["n"], row["target_cover_size"]) for row in data})
parallel_implementations = [
    implementation
    for implementation in ("fine_tuned", "sampling", "euler_tour")
    if select(implementation=implementation)
]
thread_counts = sorted(
    {
        row["threads"]
        for row in data
        if row["implementation"] in parallel_implementations
    }
)

if not parallel_implementations or not thread_counts:
    print("Error: no parallel implementation measurements found")
    sys.exit(1)

styles = {
    "fine_tuned": {
        "color": "tab:green",
        "marker": "^",
        "label": "FineTuned",
    },
    "sampling": {"color": "tab:blue", "marker": "o", "label": "Sampling"},
    "euler_tour": {
        "color": "tab:orange",
        "marker": "s",
        "label": "EulerTour",
    },
}

print("Generating implementation scaling visualizations...")
print(f"Instances (n, target): {instances}")
print(f"Thread counts: {thread_counts}")

# Execution time: both parallel implementations, with serial as a reference.
figure, axes = make_subplots("Execution Time vs Thread Count")
for axis, (n, target) in zip(axes, instances):
    serial_time = average("time_ms", "serial", n, target)
    if serial_time is not None:
        axis.axhline(serial_time, color="black", linestyle="--", label="Serial")
    for implementation in parallel_implementations:
        points = [
            (threads, average("time_ms", implementation, n, target, threads))
            for threads in thread_counts
        ]
        points = [(threads, value) for threads, value in points if value is not None]
        style = styles[implementation]
        axis.plot(
            [point[0] for point in points],
            [point[1] for point in points],
            marker=style["marker"],
            color=style["color"],
            label=style["label"],
        )
    axis.set_title(f"n = {n:,}, target = {target:,}")
    axis.set_xlabel("Threads")
    axis.set_ylabel("Time (ms, log scale)")
    axis.set_yscale("log")
    axis.set_xticks(thread_counts)
    axis.legend()
save(figure, "time_vs_threads")

# End-to-end speedup relative to the serial greedy implementation.
figure, axes = make_subplots("Speedup over Serial vs Thread Count")
for axis, (n, target) in zip(axes, instances):
    serial_time = average("time_ms", "serial", n, target)
    if serial_time is None:
        axis.text(0.5, 0.5, "No serial baseline", ha="center", va="center")
        continue
    axis.axhline(1.0, color="black", linestyle="--", label="Serial")
    for implementation in parallel_implementations:
        points = []
        for threads in thread_counts:
            parallel_time = average("time_ms", implementation, n, target, threads)
            if parallel_time is not None:
                points.append((threads, serial_time / parallel_time))
        style = styles[implementation]
        axis.plot(
            [point[0] for point in points],
            [point[1] for point in points],
            marker=style["marker"],
            color=style["color"],
            label=style["label"],
        )
    axis.set_title(f"n = {n:,}, target = {target:,}")
    axis.set_xlabel("Threads")
    axis.set_ylabel("Speedup over serial")
    axis.set_xticks(thread_counts)
    axis.legend()
save(figure, "speedup_vs_threads")

# Throughput uses the same serial reference but keeps the native M intervals/s unit.
figure, axes = make_subplots("Throughput vs Thread Count")
for axis, (n, target) in zip(axes, instances):
    serial_throughput = average(
        "throughput_M_per_sec", "serial", n, target
    )
    if serial_throughput is not None:
        axis.axhline(
            serial_throughput, color="black", linestyle="--", label="Serial"
        )
    for implementation in parallel_implementations:
        points = [
            (
                threads,
                average(
                    "throughput_M_per_sec", implementation, n, target, threads
                ),
            )
            for threads in thread_counts
        ]
        points = [(threads, value) for threads, value in points if value is not None]
        style = styles[implementation]
        axis.plot(
            [point[0] for point in points],
            [point[1] for point in points],
            marker=style["marker"],
            color=style["color"],
            label=style["label"],
        )
    axis.set_title(f"n = {n:,}, target = {target:,}")
    axis.set_xlabel("Threads")
    axis.set_ylabel("Throughput (M intervals/s)")
    axis.set_xticks(thread_counts)
    axis.legend()
save(figure, "throughput_vs_threads")

# Strong-scaling efficiency is measured relative to each implementation's own
# one-worker time, rather than to the serial algorithm.
figure, axes = make_subplots("Parallel Efficiency vs Thread Count")
for axis, (n, target) in zip(axes, instances):
    axis.axhline(100.0, color="black", linestyle="--", label="Ideal")
    for implementation in parallel_implementations:
        one_thread_time = average("time_ms", implementation, n, target, 1)
        if one_thread_time is None:
            continue
        points = []
        for threads in thread_counts:
            parallel_time = average("time_ms", implementation, n, target, threads)
            if parallel_time is not None:
                efficiency = one_thread_time / parallel_time / threads * 100.0
                points.append((threads, efficiency))
        style = styles[implementation]
        axis.plot(
            [point[0] for point in points],
            [point[1] for point in points],
            marker=style["marker"],
            color=style["color"],
            label=style["label"],
        )
    axis.set_title(f"n = {n:,}, target = {target:,}")
    axis.set_xlabel("Threads")
    axis.set_ylabel("Efficiency (%)")
    axis.set_xticks(thread_counts)
    axis.legend()
save(figure, "efficiency_vs_threads")

print("\nPerformance summary")
print("=" * 72)
for n, target in instances:
    serial_time = average("time_ms", "serial", n, target)
    actual_rows = select(n=n, target=target)
    actual = actual_rows[0]["actual_cover_size"] if actual_rows else None
    print(f"n = {n:,}, target = {target:,}, actual = {actual:,}")
    if serial_time is not None:
        print(f"  Serial: {serial_time:.3f} ms")
    for threads in thread_counts:
        entries = []
        for implementation in parallel_implementations:
            time_ms = average("time_ms", implementation, n, target, threads)
            if time_ms is None:
                continue
            comparison = (
                f", {serial_time / time_ms:.2f}x vs serial"
                if serial_time is not None
                else ""
            )
            entries.append(f"{styles[implementation]['label']} {time_ms:.3f} ms{comparison}")
        if entries:
            print(f"  {threads:2d} threads: " + "; ".join(entries))

print(f"\nPlots written to {PLOTS_DIR}")
