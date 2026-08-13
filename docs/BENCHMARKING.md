# Benchmarking

## Canonical environment

Reportable performance experiments for this repository must run on `genoa3`.
Both CPU execution and memory allocation must be bound to NUMA node 0:

```bash
numactl --cpunodebind=0 --membind=0 <command>
```

Results from another host, from an unbound process, or from a process with only
CPU or only memory binding are not comparable and must not be reported as
canonical results.

Before a run, verify the environment:

```bash
hostname
lscpu
numactl --hardware
numactl --cpunodebind=0 --membind=0 numactl --show
```

The hostname must be `genoa3`. The final command must show CPU and memory
binding to node 0.

## Release build

Initialize the dependency and build optimized benchmark binaries:

```bash
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target \
  benchmark_thread_scaling \
  benchmark_parallel_breakdown
```

Do not use Debug binaries for performance measurements.

## Configuration

Benchmark parameters live in `tools/benchmark_config.sh`:

```bash
export NUMA_NODE=0
export THREAD_COUNTS="1 2 4 8 12 16 20"
export BENCHMARK_INSTANCES="100000:100 100000:10000 1000000:1000 1000000:100000 10000000:10000 10000000:1000000"
```

Keep `NUMA_NODE=0` for canonical runs. Thread counts must not exceed the CPUs
available on node 0. Each instance uses `n:target_cover_size`; the target
controls the expected answer scale but does not force an exact result. Record
any changes to instances, thread counts, compiler, compiler flags, ParlayLib
revision, and repository commit together with the results.

The generator selects a sparse random subset of intervals as extension
candidates, with about 12 candidates in each nominal reach-sized window. A
candidate proposes a raw reach uniformly between 0.5 and 1.5 times the global
reach scale; other intervals propose only the minimum one-index advance. An
inclusive prefix maximum forms the monotone random envelope required by the
input contract. Keeping the number of competing candidates fixed prevents the
effective reaches from concentrating near the upper bound as the reach scale
grows.

The generator measures the realized cover size once and applies one global
scale correction. It never places predetermined answer positions, and the
final answer is not forced to equal the target. Random coordinate gaps and
right endpoints provide additional endpoint variation. The default seed is
`42`, making the generated input reproducible. Input generation and scale
correction occur before the timed region.

## Running

Use the unified runner for either suite or both:

```bash
./tools/run_benchmarks.sh scaling
./tools/run_benchmarks.sh breakdown
./tools/run_benchmarks.sh all
```

The runner sets `PARLAY_NUM_THREADS` for every configuration and invokes the
benchmark through:

```bash
numactl --cpunodebind=0 --membind=0
```

Each run receives a unique run ID and writes:

```text
results/thread_scaling_<run-id>.csv
results/parallel_breakdown_<run-id>.csv
plots/<run-id>/
```

The runner warns and continues if `numactl` is unavailable. Such
a fallback run is useful only for debugging; discard it for performance
analysis.

The defaults in `benchmark_config.sh` can be overridden for one invocation:

```bash
THREAD_COUNTS="1 2 4 8 16 32" RUN_ID="genoa3_trial" \
  ./tools/run_benchmarks.sh all
```

Plotting requires Python with `matplotlib` and `numpy`. Generated plots are
written below `plots/<run-id>/`. Use `--no-build` to reuse existing binaries
or `--no-plot` when only CSV output is needed.

## Measurements

`benchmark_thread_scaling` records:

- serial greedy time once for each input size;
- FineTuned, Sampling, and EulerTour time for each Parlay worker count;
- target and realized cover sizes;
- selected interval count, checked for agreement between all implementations;
- throughput in millions of intervals per second.

`benchmark_parallel_breakdown` records the Sampling phases:

- furthest-index construction;
- interval sampling;
- sampled connection construction;
- sparse sampled scan;
- parallel non-sampled scan.

Use the selected interval count as a basic correctness guard across worker
counts. Use repeated runs and a stable machine state when interpreting small
timing differences.
