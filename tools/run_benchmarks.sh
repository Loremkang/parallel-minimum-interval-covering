#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# shellcheck source=benchmark_config.sh
source "$SCRIPT_DIR/benchmark_config.sh"

usage() {
  cat <<'EOF'
Usage: tools/run_benchmarks.sh [scaling|breakdown|all] [options]

Options:
  --no-build  Use existing benchmark binaries.
  --no-plot   Produce CSV files without plots.
  -h, --help  Show this help.

Configuration is supplied through environment variables; see
tools/benchmark_config.sh. RUN_ID controls output file names.
EOF
}

MODE="all"
BUILD=true
PLOT=true

if [[ $# -gt 0 && "$1" != -* ]]; then
  MODE="$1"
  shift
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-build)
      BUILD=false
      ;;
    --no-plot)
      PLOT=false
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

case "$MODE" in
  scaling|breakdown|all) ;;
  *)
    echo "Unknown benchmark mode: $MODE" >&2
    usage >&2
    exit 2
    ;;
esac

absolute_path() {
  if [[ "$1" = /* ]]; then
    printf '%s\n' "$1"
  else
    printf '%s/%s\n' "$PROJECT_ROOT" "$1"
  fi
}

BUILD_DIR="$(absolute_path "$BUILD_DIR")"
RESULTS_DIR="$(absolute_path "$RESULTS_DIR")"
PLOTS_DIR="$(absolute_path "$PLOTS_DIR")"
VENV_PATH="$(absolute_path "$VENV_PATH")"

HOST_NAME="$(hostname -s 2>/dev/null || hostname)"
RUN_ID="${RUN_ID:-${HOST_NAME}_$(date +%Y%m%dT%H%M%S)}"
if [[ ! "$RUN_ID" =~ ^[A-Za-z0-9._-]+$ ]]; then
  echo "RUN_ID may contain only letters, digits, dot, underscore, and hyphen" >&2
  exit 2
fi

NUMA_COMMAND=()
if command -v numactl >/dev/null 2>&1; then
  NUMA_COMMAND=(numactl "--cpunodebind=$NUMA_NODE" "--membind=$NUMA_NODE")
else
  echo "WARNING: numactl is unavailable; this run is non-canonical." >&2
fi
if [[ "$HOST_NAME" != "genoa3" ]]; then
  echo "WARNING: host is $HOST_NAME, not genoa3; this run is non-canonical." >&2
fi

run_with_workers() {
  local workers="$1"
  shift
  if [[ ${#NUMA_COMMAND[@]} -gt 0 ]]; then
    env PARLAY_NUM_THREADS="$workers" "${NUMA_COMMAND[@]}" "$@"
  else
    env PARLAY_NUM_THREADS="$workers" "$@"
  fi
}

detect_thread_limit() {
  local count
  if [[ ${#NUMA_COMMAND[@]} -gt 0 ]] && command -v lscpu >/dev/null 2>&1; then
    count="$(
      lscpu -p=CORE,SOCKET,NODE 2>/dev/null |
        awk -F, -v node="$NUMA_NODE" '$1 !~ /^#/ && $3 == node {print $2 ":" $1}' |
        sort -u | wc -l | tr -d ' '
    )"
    if [[ "$count" =~ ^[1-9][0-9]*$ ]]; then
      printf '%s\n' "$count"
      return
    fi
  fi
  if command -v nproc >/dev/null 2>&1; then
    nproc
  elif command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.logicalcpu
  else
    printf '1\n'
  fi
}

THREAD_LIMIT="$(detect_thread_limit)"
read -r -a CONFIGURED_THREADS <<< "$THREAD_COUNTS"
FILTERED_THREADS=()
for threads in "${CONFIGURED_THREADS[@]}"; do
  if [[ ! "$threads" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid thread count: $threads" >&2
    exit 2
  fi
  if (( threads <= THREAD_LIMIT )); then
    FILTERED_THREADS+=("$threads")
  fi
done
if [[ ${#FILTERED_THREADS[@]} -eq 0 ]]; then
  echo "No configured thread count is within the limit $THREAD_LIMIT" >&2
  exit 2
fi

read -r -a INSTANCES <<< "$BENCHMARK_INSTANCES"
if [[ ${#INSTANCES[@]} -eq 0 ]]; then
  echo "BENCHMARK_INSTANCES must not be empty" >&2
  exit 2
fi

TARGETS=()
if [[ "$MODE" = "scaling" || "$MODE" = "all" ]]; then
  TARGETS+=(benchmark_thread_scaling)
fi
if [[ "$MODE" = "breakdown" || "$MODE" = "all" ]]; then
  TARGETS+=(benchmark_parallel_breakdown)
fi

if [[ "$BUILD" = true ]]; then
  cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$BUILD_DIR" --parallel "$THREAD_LIMIT" --target "${TARGETS[@]}"
fi

for target in "${TARGETS[@]}"; do
  if [[ ! -x "$BUILD_DIR/bin/$target" ]]; then
    echo "Benchmark binary not found: $BUILD_DIR/bin/$target" >&2
    exit 1
  fi
done

if [[ "$PLOT" = true ]]; then
  if [[ -x "$VENV_PATH/bin/python3" ]]; then
    PYTHON="$VENV_PATH/bin/python3"
  else
    PYTHON="$(command -v python3 || true)"
  fi
  if [[ -z "$PYTHON" ]]; then
    echo "python3 is required for plotting; use --no-plot to skip plots" >&2
    exit 1
  fi
fi

mkdir -p "$RESULTS_DIR" "$PLOTS_DIR/$RUN_ID"
RUN_DIRECTORY="$(mktemp -d "${TMPDIR:-/tmp}/interval-cover-benchmark.XXXXXX")"
cleanup() {
  if [[ -n "${RUN_DIRECTORY:-}" && -d "$RUN_DIRECTORY" ]]; then
    rm -rf -- "$RUN_DIRECTORY"
  fi
}
trap cleanup EXIT

run_scaling() {
  local output="$RESULTS_DIR/thread_scaling_$RUN_ID.csv"
  local plot_directory="$PLOTS_DIR/$RUN_ID/thread_scaling"
  local include_serial=true

  if [[ -e "$output" ]]; then
    echo "Refusing to overwrite existing result: $output" >&2
    exit 1
  fi

  for threads in "${FILTERED_THREADS[@]}"; do
    echo "Running implementation scaling with $threads thread(s)"
    (
      cd "$RUN_DIRECTORY"
      if [[ "$include_serial" = true ]]; then
        run_with_workers "$threads" \
          "$BUILD_DIR/bin/benchmark_thread_scaling" "${INSTANCES[@]}"
      else
        run_with_workers "$threads" \
          "$BUILD_DIR/bin/benchmark_thread_scaling" --skip-serial \
          "${INSTANCES[@]}"
      fi
    )
    include_serial=false
  done

  cp "$RUN_DIRECTORY/thread_scaling.csv" "$output"
  echo "Scaling results: $output"
  if [[ "$PLOT" = true ]]; then
    mkdir -p "$plot_directory"
    "$PYTHON" "$SCRIPT_DIR/plot_thread_scaling.py" "$output" "$plot_directory"
  fi
}

run_breakdown() {
  local output="$RESULTS_DIR/parallel_breakdown_$RUN_ID.csv"
  local plot_directory="$PLOTS_DIR/$RUN_ID/parallel_breakdown"

  if [[ -e "$output" ]]; then
    echo "Refusing to overwrite existing result: $output" >&2
    exit 1
  fi

  for threads in "${FILTERED_THREADS[@]}"; do
    echo "Running Sampling phase breakdown with $threads thread(s)"
    (
      cd "$RUN_DIRECTORY"
      run_with_workers "$threads" \
        "$BUILD_DIR/bin/benchmark_parallel_breakdown" "${INSTANCES[@]}"
    )
  done

  cp "$RUN_DIRECTORY/parallel_breakdown.csv" "$output"
  echo "Breakdown results: $output"
  if [[ "$PLOT" = true ]]; then
    mkdir -p "$plot_directory"
    "$PYTHON" "$SCRIPT_DIR/plot_parallel_breakdown.py" \
      "$output" "$plot_directory"
  fi
}

echo "Host: $HOST_NAME"
echo "Mode: $MODE"
echo "Thread limit: $THREAD_LIMIT"
echo "Thread counts: ${FILTERED_THREADS[*]}"
echo "Run ID: $RUN_ID"

if [[ "$MODE" = "scaling" || "$MODE" = "all" ]]; then
  run_scaling
fi
if [[ "$MODE" = "breakdown" || "$MODE" = "all" ]]; then
  run_breakdown
fi
