# Algorithm

## Problem contract

The library receives `n` intervals through two random-access functions,
`left(i)` and `right(i)`. The input must satisfy:

```text
left(i) <= left(i + 1)
right(i) <= right(i + 1)
left(i) < right(i)
left(i + 1) <= right(i)
```

Thus, both endpoint sequences are non-decreasing, every interval has positive
length, and consecutive intervals overlap or touch. The algorithm returns a
`parlay::sequence<bool>` whose `i`-th element indicates whether interval `i`
belongs to the canonical minimum chain from interval `0` to interval `n - 1`.

Input validation is a compile-time option. When enabled, the library performs
an additional parallel pass and throws `std::invalid_argument` if a
precondition fails:

```cpp
auto selected = interval_covering::minimum_interval_cover<
    interval_covering::Implementation::Sampling,
    interval_covering::InputValidation::Enabled>(
    n, get_left, get_right);
```

Validation is disabled by default so that valid production inputs do not pay
for an extra pass.

## Implementation selection

`Implementation::FineTuned` is the default. The dispatch logic lives directly
in `interval_covering.h`; it currently falls back to Sampling. This preserves a
stable default API while leaving one visible location for policies calibrated
on `genoa3`.

`Implementation::Serial`, `Implementation::Sampling`, and
`Implementation::EulerTour` select concrete implementations and remain useful
for reproducible experiments and correctness comparisons.

## Shared parallel preprocessing

Sampling and EulerTour first construct the furthest-index array:

```text
furthest[i] = max { j | left(j) <= right(i) }
```

Following `furthest` repeatedly gives the greedy chain that extends coverage
as far right as possible at every step.

`internal/common.h` constructs this array with a parallel divide-and-conquer
merge. It binary-searches the result for the midpoint, uses that result to
partition the two recursive candidate ranges, and evaluates the two halves
with `parlay::par_do`. Small subproblems switch to a cache-friendly serial
two-pointer scan.

## Sampling implementation

`Implementation::Sampling` operates directly on the `furthest` chain:

1. Randomly sample interval indices; always sample `0` and `n - 1`.
2. From every sampled index, follow `furthest` until reaching another sampled
   index. These searches run in parallel.
3. Scan the resulting sparse chain from `0` to `n - 1` serially.
4. Fill the non-sampled portions between consecutive valid samples in
   parallel.

Sampling only partitions the work. It does not approximate the result.

The expected work after preprocessing is linear. The sparse serial scan is
small in expectation, while the intervals between its samples form independent
parallel tasks. Random sampling means the theoretical worst case can be less
balanced than the expected case.

## Euler-tour implementation

`Implementation::EulerTour` is an alternative implementation retained for
comparison:

1. Interpret `i -> furthest[i]` as a rooted structure ending at `n - 1`.
2. Create entry and exit nodes for every interval.
3. Link the `2n` nodes into an Euler-tour list.
4. Sample and scan that list in parallel.
5. Select interval `i` when the propagated states of its entry and exit nodes
   differ.

This implementation creates more intermediate state and performs more pointer
chasing than Sampling.

To select it at compile time:

```cpp
auto selected = interval_covering::minimum_interval_cover<
    interval_covering::Implementation::EulerTour>(
    n, get_left, get_right);
```

## Serial implementation

`Implementation::Serial` exposes the greedy linear scan through the same
public function. It avoids parallel preprocessing and scheduling overhead, so
FineTuned may select it for suitable inputs after the `genoa3` policy has been
calibrated.

## Internal organization

```text
include/
├── interval_covering.h
└── interval_covering/internal/
    ├── common.h
    ├── sampling.h
    └── euler_tour.h
```

`interval_covering.h` is the public entry point. Everything below
`interval_covering::internal` is an implementation detail and carries no API
stability guarantee.

The test suite independently compares FineTuned, Serial, Sampling, EulerTour,
the default call, and a serial construction of the furthest-index array.
Correctness cross-checks are not part of the library execution path.

Input validation has a dedicated reliability suite covering valid generated
inputs, every validation error at every possible position, deterministic
first-error reporting under parallel execution, and all public implementations.
The same suite runs with Debug assertions enabled and with `-O3 -DNDEBUG`.
