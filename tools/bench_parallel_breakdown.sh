#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================="
echo "Parallel Breakdown Benchmark"
echo "========================================="

# Step 1: Run benchmark
echo ""
echo "[1/2] Running benchmark..."
"$SCRIPT_DIR/run_parallel_breakdown.sh"

# Step 2: Generate plots
echo ""
echo "[2/2] Generating plots..."
python3 "$SCRIPT_DIR/plot_parallel_breakdown.py"

echo ""
echo "========================================="
echo "Complete!"
echo "  Results: results/parallel_breakdown.csv"
echo "  Plots:   plots/breakdown_*.{png,pdf}"
echo "========================================="
