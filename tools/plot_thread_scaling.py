#!/usr/bin/env python3

import csv
import matplotlib.pyplot as plt
import matplotlib
import numpy as np
from pathlib import Path
import sys

# Use a non-interactive backend if no display available
matplotlib.use('Agg')

# Path setup
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
RESULTS_DIR = PROJECT_ROOT / 'results'
PLOTS_DIR = PROJECT_ROOT / 'plots'

# Validate input exists
INPUT_FILE = RESULTS_DIR / 'thread_scaling.csv'
if not INPUT_FILE.exists():
    print(f"Error: {INPUT_FILE} not found")
    print("Please run: tools/run_thread_scaling.sh")
    sys.exit(1)

# Create plots directory
PLOTS_DIR.mkdir(exist_ok=True)

# Read the CSV data
data = []
with open(INPUT_FILE, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        data.append({
            'algorithm': row['algorithm'],
            'n': int(row['n']),
            'threads': int(row['threads']),
            'time_ms': float(row['time_ms']),
            'num_selected': int(row['num_selected']),
            'throughput_M_per_sec': float(row['throughput_M_per_sec'])
        })

# Helper functions
def get_data(algorithm=None, n=None, threads=None):
    result = data
    if algorithm:
        result = [r for r in result if r['algorithm'] == algorithm]
    if n is not None:
        result = [r for r in result if r['n'] == n]
    if threads is not None:
        result = [r for r in result if r['threads'] == threads]
    return result

def get_unique(key, **filters):
    return sorted(list(set(r[key] for r in get_data(**filters))))


# Configure matplotlib
plt.rcParams['figure.figsize'] = (10, 6)
plt.rcParams['font.size'] = 11
plt.rcParams['lines.linewidth'] = 2
plt.rcParams['axes.grid'] = True
plt.rcParams['grid.alpha'] = 0.3

sizes = get_unique('n')
thread_counts = get_unique('threads', algorithm='parallel')

print("Generating performance visualizations...")
print(f"Input sizes: {sizes}")
print(f"Thread counts: {thread_counts}\n")

# ============================================================================
# Graph 1: Execution Time vs Input Size
# ============================================================================
print("Graph 1: Execution Time vs Input Size...")

fig, ax = plt.subplots(figsize=(12, 7))

# Serial
serial_n = [r['n'] for r in get_data(algorithm='serial')]
serial_time = [r['time_ms'] for r in get_data(algorithm='serial')]
ax.loglog(serial_n, serial_time, 'k-', linewidth=3, marker='o',
          markersize=8, label='Serial', zorder=10)

# Parallel
colors = plt.cm.viridis(np.linspace(0, 0.9, len(thread_counts)))
for threads, color in zip(thread_counts, colors):
    par_data = sorted(get_data(algorithm='parallel', threads=threads), key=lambda x: x['n'])
    if par_data:
        ns = [r['n'] for r in par_data]
        times = [r['time_ms'] for r in par_data]
        ax.loglog(ns, times, marker='s', markersize=6, color=color,
                  label=f'Parallel-{threads}-threads')

ax.set_xlabel('Input Size (intervals)', fontsize=12, fontweight='bold')
ax.set_ylabel('Time (ms, log scale)', fontsize=12, fontweight='bold')
ax.set_title('Execution Time vs Input Size', fontsize=14, fontweight='bold')
ax.legend(loc='upper left')
ax.grid(True, which='both', alpha=0.3)
plt.tight_layout()
plt.savefig(PLOTS_DIR / 'time_vs_size.png', dpi=300, bbox_inches='tight')
plt.savefig(PLOTS_DIR / 'time_vs_size.pdf', bbox_inches='tight')
print(f"  Saved: {PLOTS_DIR / 'time_vs_size.png'}\n")
plt.close()

# ============================================================================
# Graph 2: Speedup vs Thread Count
# ============================================================================
print("Graph 2: Speedup vs Thread Count...")

fig, ax = plt.subplots(figsize=(12, 7))
colors_sizes = plt.cm.plasma(np.linspace(0, 0.9, len(sizes)))

for size, color in zip(sizes, colors_sizes):
    serial_time = np.mean([r['time_ms'] for r in get_data(algorithm='serial', n=size)])
    speedups = []
    threads_list = []
    for threads in thread_counts:
        par_times = [r['time_ms'] for r in get_data(algorithm='parallel', n=size, threads=threads)]
        if par_times:
            speedup = serial_time / np.mean(par_times)
            speedups.append(speedup)
            threads_list.append(threads)
    if speedups:
        ax.plot(threads_list, speedups, marker='o', markersize=8,
                color=color, label=f'n={size:,}', linewidth=2)

ax.axhline(y=1.0, color='red', linestyle='--', linewidth=2,
           label='Serial Baseline', zorder=5)
max_threads = max(thread_counts)
ax.plot([1, max_threads], [1, max_threads], 'k:', linewidth=2,
        alpha=0.5, label='Ideal Linear')

ax.set_xlabel('Threads', fontsize=12, fontweight='bold')
ax.set_ylabel('Speedup (vs Serial)', fontsize=12, fontweight='bold')
ax.set_title('Speedup vs Thread Count (Strong Scaling)', fontsize=14, fontweight='bold')
ax.legend(loc='upper left', ncol=2)
ax.grid(True, alpha=0.3)
ax.set_xticks(thread_counts)
plt.tight_layout()
plt.savefig(PLOTS_DIR / 'speedup_vs_threads.png', dpi=300, bbox_inches='tight')
plt.savefig(PLOTS_DIR / 'speedup_vs_threads.pdf', bbox_inches='tight')
print(f"  Saved: {PLOTS_DIR / 'speedup_vs_threads.png'}\n")
plt.close()

# ============================================================================
# Graph 3: Throughput vs Thread Count
# ============================================================================
print("Graph 3: Throughput vs Thread Count...")

fig, ax = plt.subplots(figsize=(12, 7))

for size, color in zip(sizes, colors_sizes):
    serial_tp = np.mean([r['throughput_M_per_sec'] for r in get_data(algorithm='serial', n=size)])
    throughputs = []
    threads_list = []
    for threads in thread_counts:
        par_tp = [r['throughput_M_per_sec'] for r in get_data(algorithm='parallel', n=size, threads=threads)]
        if par_tp:
            throughputs.append(np.mean(par_tp))
            threads_list.append(threads)
    if throughputs:
        ax.plot(threads_list, throughputs, marker='s', markersize=8,
                color=color, label=f'Parallel n={size:,}', linewidth=2)
    ax.axhline(y=serial_tp, color=color, linestyle='--',
               linewidth=1.5, alpha=0.6)

ax.set_xlabel('Threads', fontsize=12, fontweight='bold')
ax.set_ylabel('Throughput (M intervals/sec)', fontsize=12, fontweight='bold')
ax.set_title('Throughput vs Thread Count\n(Dashed: Serial Baseline)', fontsize=14, fontweight='bold')
ax.legend(loc='upper left', ncol=2)
ax.grid(True, alpha=0.3)
ax.set_xticks(thread_counts)
plt.tight_layout()
plt.savefig(PLOTS_DIR / 'throughput_vs_threads.png', dpi=300, bbox_inches='tight')
plt.savefig(PLOTS_DIR / 'throughput_vs_threads.pdf', bbox_inches='tight')
print(f"  Saved: {PLOTS_DIR / 'throughput_vs_threads.png'}\n")
plt.close()

# ============================================================================
# Graph 4: Parallel Efficiency vs Thread Count
# ============================================================================
print("Graph 4: Parallel Efficiency vs Thread Count...")

fig, ax = plt.subplots(figsize=(12, 7))

for size, color in zip(sizes, colors_sizes):
    par_1t = [r['time_ms'] for r in get_data(algorithm='parallel', n=size, threads=1)]
    if not par_1t:
        continue
    baseline = np.mean(par_1t)

    efficiencies = []
    threads_list = []
    for threads in thread_counts:
        par_times = [r['time_ms'] for r in get_data(algorithm='parallel', n=size, threads=threads)]
        if par_times:
            speedup = baseline / np.mean(par_times)
            efficiency = (speedup / threads) * 100.0
            efficiencies.append(efficiency)
            threads_list.append(threads)
    if efficiencies:
        ax.plot(threads_list, efficiencies, marker='o', markersize=8,
                color=color, label=f'n={size:,}', linewidth=2)

ax.axhline(y=100, color='green', linestyle='--', linewidth=2,
           label='100% (Ideal)', zorder=5)
ax.set_xlabel('Threads', fontsize=12, fontweight='bold')
ax.set_ylabel('Efficiency (%)', fontsize=12, fontweight='bold')
ax.set_title('Parallel Efficiency vs Thread Count', fontsize=14, fontweight='bold')
ax.legend(loc='upper right', ncol=2)
ax.grid(True, alpha=0.3)
ax.set_xticks(thread_counts)
ax.set_ylim([0, 110])
plt.tight_layout()
plt.savefig(PLOTS_DIR / 'efficiency_vs_threads.png', dpi=300, bbox_inches='tight')
plt.savefig(PLOTS_DIR / 'efficiency_vs_threads.pdf', bbox_inches='tight')
print(f"  Saved: {PLOTS_DIR / 'efficiency_vs_threads.png'}\n")
plt.close()

# ============================================================================
# Summary
# ============================================================================
print("="*60)
print("PERFORMANCE SUMMARY")
print("="*60)

for size in sizes:
    print(f"\nInput Size: {size:,} intervals")
    print("-" * 60)
    serial_time = np.mean([r['time_ms'] for r in get_data(algorithm='serial', n=size)])
    serial_tp = np.mean([r['throughput_M_per_sec'] for r in get_data(algorithm='serial', n=size)])
    print(f"  Serial: {serial_time:.2f} ms ({serial_tp:.1f} M/s)")

    for threads in thread_counts:
        par_data = get_data(algorithm='parallel', n=size, threads=threads)
        if par_data:
            par_time = np.mean([r['time_ms'] for r in par_data])
            par_tp = np.mean([r['throughput_M_per_sec'] for r in par_data])
            speedup = serial_time / par_time
            if speedup < 1.0:
                overhead = (par_time / serial_time - 1) * 100
                print(f"  {threads:2d} threads: {par_time:7.2f} ms ({par_tp:.1f} M/s) - {overhead:+.0f}% slower")
            else:
                print(f"  {threads:2d} threads: {par_time:7.2f} ms ({par_tp:.1f} M/s) - {speedup:.2f}x speedup")

print("\n" + "="*60)
print("All graphs generated successfully in ./plots/")
print("="*60)
