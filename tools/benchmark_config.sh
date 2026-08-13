#!/bin/bash
# Defaults for tools/run_benchmarks.sh. Every value can be overridden through
# the environment without editing this file.

: "${NUMA_NODE:=0}"
: "${THREAD_COUNTS:=1 2 4 8 12 16 20}"
: "${BENCHMARK_INSTANCES:=100000:100 100000:10000 1000000:1000 1000000:100000 10000000:10000 10000000:1000000}"
: "${BUILD_DIR:=build}"
: "${RESULTS_DIR:=results}"
: "${PLOTS_DIR:=plots}"
: "${VENV_PATH:=venv}"
