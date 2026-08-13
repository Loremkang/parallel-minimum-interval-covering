#!/usr/bin/env python3

import csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path
import sys

# Path setup
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
if not 2 <= len(sys.argv) <= 3:
    print(f"Usage: {sys.argv[0]} [input.csv] [output-directory]")
    sys.exit(1)
INPUT_FILE = Path(sys.argv[1]).resolve()
PLOTS_DIR = Path(sys.argv[2]).resolve() if len(sys.argv) == 3 else PROJECT_ROOT / 'plots'

# Validate input exists
if not INPUT_FILE.exists():
    print(f"Error: {INPUT_FILE} not found")
    print("Please run: tools/run_benchmarks.sh breakdown")
    sys.exit(1)

# Create plots directory
PLOTS_DIR.mkdir(exist_ok=True)

# Read the CSV data
data = []
with open(INPUT_FILE, 'r') as f:
    reader = csv.DictReader(f)
    required_columns = {
        'n', 'target_cover_size', 'actual_cover_size', 'threads',
        'build_furthest_ms', 'sample_intervals_ms',
        'build_connections_ms', 'scan_samples_ms', 'scan_nonsample_ms',
        'total_ms'
    }
    if not required_columns.issubset(reader.fieldnames or []):
        print(f"Error: {INPUT_FILE} uses an obsolete or invalid CSV format")
        print("Please rerun: tools/run_benchmarks.sh breakdown")
        sys.exit(1)
    for row in reader:
        data.append({
            'n': int(row['n']),
            'target_cover_size': int(row['target_cover_size']),
            'actual_cover_size': int(row['actual_cover_size']),
            'threads': int(row['threads']),
            'build_furthest_ms': float(row['build_furthest_ms']),
            'sample_intervals_ms': float(row['sample_intervals_ms']),
            'build_connections_ms': float(row['build_connections_ms']),
            'scan_samples_ms': float(row['scan_samples_ms']),
            'scan_nonsample_ms': float(row['scan_nonsample_ms']),
            'total_ms': float(row['total_ms'])
        })

# Helper functions
def get_data(n=None, target=None, threads=None):
    result = data
    if n is not None:
        result = [r for r in result if r['n'] == n]
    if target is not None:
        result = [r for r in result if r['target_cover_size'] == target]
    if threads is not None:
        result = [r for r in result if r['threads'] == threads]
    return result

def get_unique(key):
    return sorted(list(set(r[key] for r in data)))

def safe_speedup(baseline, current):
    if current == 0:
        return 1.0 if baseline == 0 else float('nan')
    return baseline / current


# Configure matplotlib
plt.rcParams['figure.figsize'] = (12, 7)
plt.rcParams['font.size'] = 11
plt.rcParams['lines.linewidth'] = 2
plt.rcParams['axes.grid'] = True
plt.rcParams['grid.alpha'] = 0.3

instances = sorted(set((r['n'], r['target_cover_size']) for r in data))
thread_counts = get_unique('threads')

print("Generating breakdown visualizations...")
print(f"Instances (n, target): {instances}")
print(f"Thread counts: {thread_counts}\n")

# ============================================================================
# Graph 1: Stacked Bar Chart - Time Breakdown by Thread Count (largest size)
# ============================================================================

# Focus on the largest (n, target) instance for readable phase plots.
n_focus, target_focus = max(instances)
focus_label = f"n={n_focus:,}, target={target_focus:,}"
print(f"Graph 1: Stacked Bar Chart - Phase Breakdown ({focus_label})...")

fig, ax = plt.subplots(figsize=(12, 7))

# Focus on largest size to see bottlenecks clearly
breakdown_data = sorted(
    get_data(n=n_focus, target=target_focus), key=lambda x: x['threads'])

threads = [r['threads'] for r in breakdown_data]
build_furthest = [r['build_furthest_ms'] for r in breakdown_data]
sample_intervals = [r['sample_intervals_ms'] for r in breakdown_data]
build_connections = [r['build_connections_ms'] for r in breakdown_data]
scan_samples = [r['scan_samples_ms'] for r in breakdown_data]
scan_nonsample = [r['scan_nonsample_ms'] for r in breakdown_data]

x = np.arange(len(threads))
width = 0.6

p1 = ax.bar(x, build_furthest, width, label='BuildFurthest', color='#3498db')
p2 = ax.bar(x, sample_intervals, width, bottom=build_furthest, label='SampleIntervals', color='#e74c3c')
p3 = ax.bar(x, build_connections, width,
            bottom=np.array(build_furthest) + np.array(sample_intervals),
            label='BuildConnections', color='#2ecc71')
p4 = ax.bar(x, scan_samples, width,
            bottom=np.array(build_furthest) + np.array(sample_intervals) + np.array(build_connections),
            label='ScanSamples', color='#f39c12')
p5 = ax.bar(x, scan_nonsample, width,
            bottom=np.array(build_furthest) + np.array(sample_intervals) + np.array(build_connections) + np.array(scan_samples),
            label='ScanNonsample', color='#9b59b6')

ax.set_xlabel('Thread Count', fontsize=12, fontweight='bold')
ax.set_ylabel('Time (ms)', fontsize=12, fontweight='bold')
ax.set_title(f'Sampling Phase Breakdown ({focus_label})', fontsize=14, fontweight='bold')
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
# Graph 2: Line Graph - Each Phase Scaling with Thread Count (largest size)
# ============================================================================
print(f"Graph 2: Phase Scaling with Thread Count ({focus_label})...")

fig, ax = plt.subplots(figsize=(12, 7))

breakdown_data = sorted(
    get_data(n=n_focus, target=target_focus), key=lambda x: x['threads'])
threads = [r['threads'] for r in breakdown_data]
build_furthest = [r['build_furthest_ms'] for r in breakdown_data]
sample_intervals = [r['sample_intervals_ms'] for r in breakdown_data]
build_connections = [r['build_connections_ms'] for r in breakdown_data]
scan_samples = [r['scan_samples_ms'] for r in breakdown_data]
scan_nonsample = [r['scan_nonsample_ms'] for r in breakdown_data]

ax.plot(threads, build_furthest, marker='o', markersize=8, linewidth=2,
        label='BuildFurthest', color='#3498db')
ax.plot(threads, sample_intervals, marker='s', markersize=8, linewidth=2,
        label='SampleIntervals', color='#e74c3c')
ax.plot(threads, build_connections, marker='^', markersize=8, linewidth=2,
        label='BuildConnections', color='#2ecc71')
ax.plot(threads, scan_samples, marker='d', markersize=8, linewidth=2,
        label='ScanSamples', color='#f39c12')
ax.plot(threads, scan_nonsample, marker='*', markersize=10, linewidth=2,
        label='ScanNonsample', color='#9b59b6')

ax.set_xlabel('Thread Count', fontsize=12, fontweight='bold')
ax.set_ylabel('Time (ms)', fontsize=12, fontweight='bold')
ax.set_title(f'Phase Execution Time vs Thread Count ({focus_label})', fontsize=14, fontweight='bold')
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
print(f"Graph 3: Percentage Breakdown by Thread Count ({focus_label})...")

fig, ax = plt.subplots(figsize=(12, 7))

breakdown_data = sorted(
    get_data(n=n_focus, target=target_focus), key=lambda x: x['threads'])
threads = [r['threads'] for r in breakdown_data]

# Calculate percentages
build_furthest_pct = [r['build_furthest_ms'] / r['total_ms'] * 100 for r in breakdown_data]
sample_intervals_pct = [r['sample_intervals_ms'] / r['total_ms'] * 100 for r in breakdown_data]
build_connections_pct = [r['build_connections_ms'] / r['total_ms'] * 100 for r in breakdown_data]
scan_samples_pct = [r['scan_samples_ms'] / r['total_ms'] * 100 for r in breakdown_data]
scan_nonsample_pct = [r['scan_nonsample_ms'] / r['total_ms'] * 100 for r in breakdown_data]

x = np.arange(len(threads))
width = 0.6

p1 = ax.bar(x, build_furthest_pct, width, label='BuildFurthest', color='#3498db')
p2 = ax.bar(x, sample_intervals_pct, width, bottom=build_furthest_pct,
            label='SampleIntervals', color='#e74c3c')
p3 = ax.bar(x, build_connections_pct, width,
            bottom=np.array(build_furthest_pct) + np.array(sample_intervals_pct),
            label='BuildConnections', color='#2ecc71')
p4 = ax.bar(x, scan_samples_pct, width,
            bottom=np.array(build_furthest_pct) + np.array(sample_intervals_pct) + np.array(build_connections_pct),
            label='ScanSamples', color='#f39c12')
p5 = ax.bar(x, scan_nonsample_pct, width,
            bottom=np.array(build_furthest_pct) + np.array(sample_intervals_pct) + np.array(build_connections_pct) + np.array(scan_samples_pct),
            label='ScanNonsample', color='#9b59b6')

ax.set_xlabel('Thread Count', fontsize=12, fontweight='bold')
ax.set_ylabel('Percentage of Total Time (%)', fontsize=12, fontweight='bold')
ax.set_title(f'Phase Percentage Breakdown ({focus_label})', fontsize=14, fontweight='bold')
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
print(f"Graph 4: Phase Speedup vs Thread Count ({focus_label})...")

fig, ax = plt.subplots(figsize=(12, 7))

breakdown_data = sorted(
    get_data(n=n_focus, target=target_focus), key=lambda x: x['threads'])
threads = [r['threads'] for r in breakdown_data]

# Get 1-thread baseline
baseline = get_data(n=n_focus, target=target_focus, threads=1)[0]
baseline_build_furthest = baseline['build_furthest_ms']
baseline_sample_intervals = baseline['sample_intervals_ms']
baseline_build_connections = baseline['build_connections_ms']
baseline_scan_samples = baseline['scan_samples_ms']
baseline_scan_nonsample = baseline['scan_nonsample_ms']

# Calculate speedups
build_furthest_speedup = [safe_speedup(baseline_build_furthest, r['build_furthest_ms']) for r in breakdown_data]
sample_intervals_speedup = [safe_speedup(baseline_sample_intervals, r['sample_intervals_ms']) for r in breakdown_data]
build_connections_speedup = [safe_speedup(baseline_build_connections, r['build_connections_ms']) for r in breakdown_data]
scan_samples_speedup = [safe_speedup(baseline_scan_samples, r['scan_samples_ms']) for r in breakdown_data]
scan_nonsample_speedup = [safe_speedup(baseline_scan_nonsample, r['scan_nonsample_ms']) for r in breakdown_data]

ax.plot(threads, build_furthest_speedup, marker='o', markersize=8, linewidth=2,
        label='BuildFurthest', color='#3498db')
ax.plot(threads, sample_intervals_speedup, marker='s', markersize=8, linewidth=2,
        label='SampleIntervals', color='#e74c3c')
ax.plot(threads, build_connections_speedup, marker='^', markersize=8, linewidth=2,
        label='BuildConnections', color='#2ecc71')
ax.plot(threads, scan_samples_speedup, marker='d', markersize=8, linewidth=2,
        label='ScanSamples', color='#f39c12')
ax.plot(threads, scan_nonsample_speedup, marker='*', markersize=10, linewidth=2,
        label='ScanNonsample', color='#9b59b6')

# Ideal linear speedup
ax.plot(threads, threads, 'k:', linewidth=2, alpha=0.5, label='Ideal Linear')

ax.set_xlabel('Thread Count', fontsize=12, fontweight='bold')
ax.set_ylabel('Speedup (vs 1 thread)', fontsize=12, fontweight='bold')
ax.set_title(f'Phase Speedup vs Thread Count ({focus_label})', fontsize=14, fontweight='bold')
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
print(f"BREAKDOWN ANALYSIS SUMMARY ({focus_label})")
print("="*70)

breakdown_data = sorted(
    get_data(n=n_focus, target=target_focus), key=lambda x: x['threads'])
baseline = breakdown_data[0]

print(f"\nBaseline (1 thread):")
print(f"  Total: {baseline['total_ms']:.2f} ms")
print(f"    BuildFurthest:     {baseline['build_furthest_ms']:7.2f} ms ({baseline['build_furthest_ms']/baseline['total_ms']*100:5.1f}%)")
print(f"    SampleIntervals:   {baseline['sample_intervals_ms']:7.2f} ms ({baseline['sample_intervals_ms']/baseline['total_ms']*100:5.1f}%)")
print(f"    BuildConnections:  {baseline['build_connections_ms']:7.2f} ms ({baseline['build_connections_ms']/baseline['total_ms']*100:5.1f}%)")
print(f"    ScanSamples:       {baseline['scan_samples_ms']:7.2f} ms ({baseline['scan_samples_ms']/baseline['total_ms']*100:5.1f}%)")
print(f"    ScanNonsample:     {baseline['scan_nonsample_ms']:7.2f} ms ({baseline['scan_nonsample_ms']/baseline['total_ms']*100:5.1f}%)")

print(f"\n{'Threads':<8} {'BuildFurthest':<17} {'SampleIntervals':<17} {'BuildConnections':<17} {'ScanSamples':<15} {'ScanNonsample':<15} {'Total':<10}")
print("-" * 105)

for r in breakdown_data:
    t = r['threads']
    build_furthest_sp = safe_speedup(baseline['build_furthest_ms'], r['build_furthest_ms'])
    sample_intervals_sp = safe_speedup(baseline['sample_intervals_ms'], r['sample_intervals_ms'])
    build_connections_sp = safe_speedup(baseline['build_connections_ms'], r['build_connections_ms'])
    scan_samples_sp = safe_speedup(baseline['scan_samples_ms'], r['scan_samples_ms'])
    scan_nonsample_sp = safe_speedup(baseline['scan_nonsample_ms'], r['scan_nonsample_ms'])
    total_sp = safe_speedup(baseline['total_ms'], r['total_ms'])

    print(f"{t:<8} {build_furthest_sp:>6.2f}x          {sample_intervals_sp:>6.2f}x          {build_connections_sp:>6.2f}x          {scan_samples_sp:>6.2f}x        {scan_nonsample_sp:>6.2f}x        {total_sp:>6.2f}x")

print("\n" + "="*70)
print("KEY FINDINGS:")
print("="*70)

# Find best configuration
best = max(breakdown_data, key=lambda x: baseline['total_ms'] / x['total_ms'])
print(f"\n1. Best Configuration: {best['threads']} threads")
print(f"   - Total speedup: {baseline['total_ms'] / best['total_ms']:.2f}x")
print(f"   - Time: {best['total_ms']:.2f} ms (down from {baseline['total_ms']:.2f} ms)")

# Identify bottleneck
bottleneck_phase = max(['build_furthest', 'sample_intervals', 'build_connections', 'scan_samples', 'scan_nonsample'],
                       key=lambda phase: baseline[f'{phase}_ms'])
print(f"\n2. Primary Bottleneck: {bottleneck_phase.replace('_', ' ').title()}")
print(f"   - Takes {baseline[f'{bottleneck_phase}_ms']/baseline['total_ms']*100:.1f}% of total time (1 thread)")
print(f"   - Takes {best[f'{bottleneck_phase}_ms']/best['total_ms']*100:.1f}% of total time ({best['threads']} threads)")

# Analyze scaling
print(f"\n3. Phase Scaling Analysis (1 → {best['threads']} threads):")
for phase_name, phase_key in [('BuildFurthest', 'build_furthest_ms'),
                               ('SampleIntervals', 'sample_intervals_ms'),
                               ('BuildConnections', 'build_connections_ms'),
                               ('ScanSamples', 'scan_samples_ms'),
                               ('ScanNonsample', 'scan_nonsample_ms')]:
    speedup = safe_speedup(baseline[phase_key], best[phase_key])
    efficiency = (speedup / best['threads']) * 100
    print(f"   - {phase_name:<17}: {speedup:5.2f}x speedup ({efficiency:5.1f}% efficiency)")

print("\n" + "="*70)
print("All breakdown graphs generated successfully in ./plots/")
print("="*70)
