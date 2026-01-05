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
INPUT_FILE = RESULTS_DIR / 'parallel_breakdown.csv'
if not INPUT_FILE.exists():
    print(f"Error: {INPUT_FILE} not found")
    print("Please run: tools/run_parallel_breakdown.sh")
    sys.exit(1)

# Create plots directory
PLOTS_DIR.mkdir(exist_ok=True)

# Read the CSV data
data = []
with open(INPUT_FILE, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        data.append({
            'n': int(row['n']),
            'threads': int(row['threads']),
            'find_furthest_ms': float(row['find_furthest_ms']),
            'build_linklist_ms': float(row['build_linklist_ms']),
            'scan_linklist_ms': float(row['scan_linklist_ms']),
            'extract_valid_ms': float(row['extract_valid_ms']),
            'total_ms': float(row['total_ms'])
        })

# Helper functions
def get_data(n=None, threads=None):
    result = data
    if n is not None:
        result = [r for r in result if r['n'] == n]
    if threads is not None:
        result = [r for r in result if r['threads'] == threads]
    return result

def get_unique(key):
    return sorted(list(set(r[key] for r in data)))


# Configure matplotlib
plt.rcParams['figure.figsize'] = (12, 7)
plt.rcParams['font.size'] = 11
plt.rcParams['lines.linewidth'] = 2
plt.rcParams['axes.grid'] = True
plt.rcParams['grid.alpha'] = 0.3

sizes = get_unique('n')
thread_counts = get_unique('threads')

print("Generating breakdown visualizations...")
print(f"Input sizes: {sizes}")
print(f"Thread counts: {thread_counts}\n")

# ============================================================================
# Graph 1: Stacked Bar Chart - Time Breakdown by Thread Count (10M intervals)
# ============================================================================
print("Graph 1: Stacked Bar Chart - Phase Breakdown (n=10M)...")

fig, ax = plt.subplots(figsize=(12, 7))

# Focus on largest size to see bottlenecks clearly
n_focus = 10000000
breakdown_data = sorted(get_data(n=n_focus), key=lambda x: x['threads'])

threads = [r['threads'] for r in breakdown_data]
find_furthest = [r['find_furthest_ms'] for r in breakdown_data]
build_linklist = [r['build_linklist_ms'] for r in breakdown_data]
scan_linklist = [r['scan_linklist_ms'] for r in breakdown_data]
extract_valid = [r['extract_valid_ms'] for r in breakdown_data]

x = np.arange(len(threads))
width = 0.6

p1 = ax.bar(x, find_furthest, width, label='FindFurthest', color='#3498db')
p2 = ax.bar(x, build_linklist, width, bottom=find_furthest, label='BuildLinkList', color='#e74c3c')
p3 = ax.bar(x, scan_linklist, width,
            bottom=np.array(find_furthest) + np.array(build_linklist),
            label='ScanLinkList', color='#2ecc71')
p4 = ax.bar(x, extract_valid, width,
            bottom=np.array(find_furthest) + np.array(build_linklist) + np.array(scan_linklist),
            label='ExtractValid', color='#f39c12')

ax.set_xlabel('Thread Count', fontsize=12, fontweight='bold')
ax.set_ylabel('Time (ms)', fontsize=12, fontweight='bold')
ax.set_title(f'KernelParallel Phase Breakdown (n={n_focus:,})', fontsize=14, fontweight='bold')
ax.set_xticks(x)
ax.set_xticklabels(threads)
ax.legend(loc='upper right')
ax.grid(True, axis='y', alpha=0.3)

plt.tight_layout()
plt.savefig(PLOTS_DIR / 'breakdown_stacked.png', dpi=300, bbox_inches='tight')
plt.savefig(PLOTS_DIR / 'breakdown_stacked.pdf', bbox_inches='tight')
print(f"  Saved: {PLOTS_DIR / 'breakdown_stacked.png'}\n")
plt.close()

# ============================================================================
# Graph 2: Line Graph - Each Phase Scaling with Thread Count (10M intervals)
# ============================================================================
print("Graph 2: Phase Scaling with Thread Count (n=10M)...")

fig, ax = plt.subplots(figsize=(12, 7))

breakdown_data = sorted(get_data(n=n_focus), key=lambda x: x['threads'])
threads = [r['threads'] for r in breakdown_data]
find_furthest = [r['find_furthest_ms'] for r in breakdown_data]
build_linklist = [r['build_linklist_ms'] for r in breakdown_data]
scan_linklist = [r['scan_linklist_ms'] for r in breakdown_data]
extract_valid = [r['extract_valid_ms'] for r in breakdown_data]

ax.plot(threads, find_furthest, marker='o', markersize=8, linewidth=2,
        label='FindFurthest', color='#3498db')
ax.plot(threads, build_linklist, marker='s', markersize=8, linewidth=2,
        label='BuildLinkList', color='#e74c3c')
ax.plot(threads, scan_linklist, marker='^', markersize=8, linewidth=2,
        label='ScanLinkList', color='#2ecc71')
ax.plot(threads, extract_valid, marker='d', markersize=8, linewidth=2,
        label='ExtractValid', color='#f39c12')

ax.set_xlabel('Thread Count', fontsize=12, fontweight='bold')
ax.set_ylabel('Time (ms)', fontsize=12, fontweight='bold')
ax.set_title(f'Phase Execution Time vs Thread Count (n={n_focus:,})', fontsize=14, fontweight='bold')
ax.legend(loc='upper right')
ax.grid(True, alpha=0.3)
ax.set_xticks(threads)

plt.tight_layout()
plt.savefig(PLOTS_DIR / 'breakdown_scaling.png', dpi=300, bbox_inches='tight')
plt.savefig(PLOTS_DIR / 'breakdown_scaling.pdf', bbox_inches='tight')
print(f"  Saved: {PLOTS_DIR / 'breakdown_scaling.png'}\n")
plt.close()

# ============================================================================
# Graph 3: Percentage Breakdown (Normalized to 100%)
# ============================================================================
print("Graph 3: Percentage Breakdown by Thread Count (n=10M)...")

fig, ax = plt.subplots(figsize=(12, 7))

breakdown_data = sorted(get_data(n=n_focus), key=lambda x: x['threads'])
threads = [r['threads'] for r in breakdown_data]

# Calculate percentages
find_furthest_pct = [r['find_furthest_ms'] / r['total_ms'] * 100 for r in breakdown_data]
build_linklist_pct = [r['build_linklist_ms'] / r['total_ms'] * 100 for r in breakdown_data]
scan_linklist_pct = [r['scan_linklist_ms'] / r['total_ms'] * 100 for r in breakdown_data]
extract_valid_pct = [r['extract_valid_ms'] / r['total_ms'] * 100 for r in breakdown_data]

x = np.arange(len(threads))
width = 0.6

p1 = ax.bar(x, find_furthest_pct, width, label='FindFurthest', color='#3498db')
p2 = ax.bar(x, build_linklist_pct, width, bottom=find_furthest_pct,
            label='BuildLinkList', color='#e74c3c')
p3 = ax.bar(x, scan_linklist_pct, width,
            bottom=np.array(find_furthest_pct) + np.array(build_linklist_pct),
            label='ScanLinkList', color='#2ecc71')
p4 = ax.bar(x, extract_valid_pct, width,
            bottom=np.array(find_furthest_pct) + np.array(build_linklist_pct) + np.array(scan_linklist_pct),
            label='ExtractValid', color='#f39c12')

ax.set_xlabel('Thread Count', fontsize=12, fontweight='bold')
ax.set_ylabel('Percentage of Total Time (%)', fontsize=12, fontweight='bold')
ax.set_title(f'Phase Percentage Breakdown (n={n_focus:,})', fontsize=14, fontweight='bold')
ax.set_xticks(x)
ax.set_xticklabels(threads)
ax.legend(loc='upper right')
ax.grid(True, axis='y', alpha=0.3)
ax.set_ylim([0, 100])

plt.tight_layout()
plt.savefig(PLOTS_DIR / 'breakdown_percentage.png', dpi=300, bbox_inches='tight')
plt.savefig(PLOTS_DIR / 'breakdown_percentage.pdf', bbox_inches='tight')
print(f"  Saved: {PLOTS_DIR / 'breakdown_percentage.png'}\n")
plt.close()

# ============================================================================
# Graph 4: Speedup of Each Phase (relative to 1 thread)
# ============================================================================
print("Graph 4: Phase Speedup vs Thread Count (n=10M)...")

fig, ax = plt.subplots(figsize=(12, 7))

breakdown_data = sorted(get_data(n=n_focus), key=lambda x: x['threads'])
threads = [r['threads'] for r in breakdown_data]

# Get 1-thread baseline
baseline = get_data(n=n_focus, threads=1)[0]
baseline_find = baseline['find_furthest_ms']
baseline_build = baseline['build_linklist_ms']
baseline_scan = baseline['scan_linklist_ms']
baseline_extract = baseline['extract_valid_ms']

# Calculate speedups
find_speedup = [baseline_find / r['find_furthest_ms'] for r in breakdown_data]
build_speedup = [baseline_build / r['build_linklist_ms'] for r in breakdown_data]
scan_speedup = [baseline_scan / r['scan_linklist_ms'] for r in breakdown_data]
extract_speedup = [baseline_extract / r['extract_valid_ms'] for r in breakdown_data]

ax.plot(threads, find_speedup, marker='o', markersize=8, linewidth=2,
        label='FindFurthest', color='#3498db')
ax.plot(threads, build_speedup, marker='s', markersize=8, linewidth=2,
        label='BuildLinkList', color='#e74c3c')
ax.plot(threads, scan_speedup, marker='^', markersize=8, linewidth=2,
        label='ScanLinkList', color='#2ecc71')
ax.plot(threads, extract_speedup, marker='d', markersize=8, linewidth=2,
        label='ExtractValid', color='#f39c12')

# Ideal linear speedup
ax.plot(threads, threads, 'k:', linewidth=2, alpha=0.5, label='Ideal Linear')

ax.set_xlabel('Thread Count', fontsize=12, fontweight='bold')
ax.set_ylabel('Speedup (vs 1 thread)', fontsize=12, fontweight='bold')
ax.set_title(f'Phase Speedup vs Thread Count (n={n_focus:,})', fontsize=14, fontweight='bold')
ax.legend(loc='upper left')
ax.grid(True, alpha=0.3)
ax.set_xticks(threads)

plt.tight_layout()
plt.savefig(PLOTS_DIR / 'breakdown_speedup.png', dpi=300, bbox_inches='tight')
plt.savefig(PLOTS_DIR / 'breakdown_speedup.pdf', bbox_inches='tight')
print(f"  Saved: {PLOTS_DIR / 'breakdown_speedup.png'}\n")
plt.close()

# ============================================================================
# Summary Statistics
# ============================================================================
print("="*70)
print("BREAKDOWN ANALYSIS SUMMARY (n=10,000,000)")
print("="*70)

breakdown_data = sorted(get_data(n=n_focus), key=lambda x: x['threads'])
baseline = breakdown_data[0]

print(f"\nBaseline (1 thread):")
print(f"  Total: {baseline['total_ms']:.2f} ms")
print(f"    FindFurthest:  {baseline['find_furthest_ms']:7.2f} ms ({baseline['find_furthest_ms']/baseline['total_ms']*100:5.1f}%)")
print(f"    BuildLinkList: {baseline['build_linklist_ms']:7.2f} ms ({baseline['build_linklist_ms']/baseline['total_ms']*100:5.1f}%)")
print(f"    ScanLinkList:  {baseline['scan_linklist_ms']:7.2f} ms ({baseline['scan_linklist_ms']/baseline['total_ms']*100:5.1f}%)")
print(f"    ExtractValid:  {baseline['extract_valid_ms']:7.2f} ms ({baseline['extract_valid_ms']/baseline['total_ms']*100:5.1f}%)")

print(f"\n{'Threads':<8} {'FindFurthest':<15} {'BuildLinkList':<15} {'ScanLinkList':<15} {'ExtractValid':<15} {'Total':<10}")
print("-" * 80)

for r in breakdown_data:
    t = r['threads']
    find_sp = baseline['find_furthest_ms'] / r['find_furthest_ms']
    build_sp = baseline['build_linklist_ms'] / r['build_linklist_ms']
    scan_sp = baseline['scan_linklist_ms'] / r['scan_linklist_ms']
    extract_sp = baseline['extract_valid_ms'] / r['extract_valid_ms']
    total_sp = baseline['total_ms'] / r['total_ms']

    print(f"{t:<8} {find_sp:>6.2f}x        {build_sp:>6.2f}x        {scan_sp:>6.2f}x        {extract_sp:>6.2f}x        {total_sp:>6.2f}x")

print("\n" + "="*70)
print("KEY FINDINGS:")
print("="*70)

# Find best configuration
best = max(breakdown_data, key=lambda x: baseline['total_ms'] / x['total_ms'])
print(f"\n1. Best Configuration: {best['threads']} threads")
print(f"   - Total speedup: {baseline['total_ms'] / best['total_ms']:.2f}x")
print(f"   - Time: {best['total_ms']:.2f} ms (down from {baseline['total_ms']:.2f} ms)")

# Identify bottleneck
bottleneck_phase = max(['find_furthest', 'build_linklist', 'scan_linklist', 'extract_valid'],
                       key=lambda phase: baseline[f'{phase}_ms'])
print(f"\n2. Primary Bottleneck: {bottleneck_phase.replace('_', ' ').title()}")
print(f"   - Takes {baseline[f'{bottleneck_phase}_ms']/baseline['total_ms']*100:.1f}% of total time (1 thread)")
print(f"   - Takes {best[f'{bottleneck_phase}_ms']/best['total_ms']*100:.1f}% of total time ({best['threads']} threads)")

# Analyze scaling
print(f"\n3. Phase Scaling Analysis (1 → {best['threads']} threads):")
for phase_name, phase_key in [('FindFurthest', 'find_furthest_ms'),
                               ('BuildLinkList', 'build_linklist_ms'),
                               ('ScanLinkList', 'scan_linklist_ms'),
                               ('ExtractValid', 'extract_valid_ms')]:
    speedup = baseline[phase_key] / best[phase_key]
    efficiency = (speedup / best['threads']) * 100
    print(f"   - {phase_name:<15}: {speedup:5.2f}x speedup ({efficiency:5.1f}% efficiency)")

print("\n" + "="*70)
print("All breakdown graphs generated successfully in ./plots/")
print("="*70)
