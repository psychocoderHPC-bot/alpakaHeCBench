<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# haversine (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`haversine` benchmark. One C++20 translation unit computes the great-circle distance between a
large table of query locations and a small set of reference cities with the haversine formula. The
same source drives the host CPU executor and the CUDA backend; there is no backend branch and no
native CUDA/HIP/OpenMP kernel.

## Provenance

- HeCBench reference: commit
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/commit/0a17ee47ca6072192101036d4bf292e6612f22e6)
  (`src/haversine-cuda/main.cu`, `distance.cu`, `distance.h`; OpenMP variant `src/haversine-omp`).
  The NVIDIA sources carry `Copyright (c) 2020, NVIDIA CORPORATION.` under Apache-2.0. They are
  kept outside this repository; only the clean-room alpaka re-implementation is committed here
  under BSD-3-Clause.
- Original alpaka port: alpaka3 PR #56 head `9ef3512eb312` (`benchmark/haversine`).
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the
  `extern/alpaka` submodule.

## Computed quantity

Each work item reads one pair `(a_lat, a_lon, b_lat, b_lon)` in `double` and evaluates

```text
ax = a_lon * DEGREE_TO_RADIAN ; ay = a_lat * DEGREE_TO_RADIAN
bx = b_lon * DEGREE_TO_RADIAN ; by = b_lat * DEGREE_TO_RADIAN
x  = (bx - ax) / 2            ; y  = (by - ay) / 2
d  = 2 * EARTH_RADIUS_KM * asin( sqrt( sin(y)^2 + sin(x)^2 * cos(ay) * cos(by) ) )
```

with `DEGREE_TO_RADIAN = pi/180` and `EARTH_RADIUS_KM = 6371`.

The input construction reproduces the pinned reference. `num_cities` distinct locations are
replicated once per reference city (`num_ref_cities = 6`), and for each block the reference-city
latitude and longitude are copied into the `z`/`w` slots using the pinned 1-based `index_map`
`{436483, 1952407, 627919, 377884, 442703, 1863423}` (bombay, melbourne, waltham, moscow,
glasgow, morocco). One work item produces one distance; there is no reduction or reordering.

Parallelism uses `onAcc::makeIdxMap` over a flat 1D `IdxRange` with
`worker::linearThreadsInGrid`. That form is correct on the host CPU executors as well as on CUDA;
a multidimensional `threadsInGrid` range is not honoured by the host executors. The frame extent
(`--frame`, default 256) is a launch-shape hint only.

By default the benchmark generates deterministic coordinates with a xorshift64* generator, so it
is self-contained and does not need the external HeCBench `locations.txt`. Pass `--input FILE` to
replay the pinned `lat lon` file instead.

## Command line

Named form:

```text
haversine --cities N --repeat R --samples S --seed X [--frame F] [--input FILE] [--dump FILE]
          [--verify-only]
```

- `--cities` default `2097152` (`2^21`, the pinned `num_cities`); `num_ref_cities` is fixed at 6.
- `--repeat` default `100`, timed kernel repetitions per sample.
- `--samples` default `5`, independent measured samples.
- `--seed` default `12345`, used by the built-in location generator.
- `--input FILE` reads the HeCBench `lat lon` format (one pair per line).
- `--dump FILE` writes the raw device distance array (validation aid).
- `--verify-only` runs the correctness check only, with no timing.
- `--frame F` sets the logical frame extent.

The HeCBench positional form is also accepted:

```text
haversine <file> <repeat>
```

Invalid values and unknown options print one `Error:` line to stderr and exit non-zero. The values
`--cities`, `--repeat`, `--samples` and `--frame` must be greater than zero.

## Verification and test semantics

`hostReference` (`src/helper.hpp`) is an independent double-precision transcription of the pinned
formula evaluated in linear order. The accelerator result is compared against it elementwise.
`verify` rejects NaN/Inf in either buffer, reports the maximum absolute deviation and returns a
non-zero exit code when any element is outside

```text
|got - ref| <= 1e-6 km + 1e-9 * |ref|
```

Both sides evaluate the same `double` expression; the only difference is the libm implementation
of `sin`/`cos`/`asin`/`sqrt`, which differs by a few ULP. The tolerance is not widened to hide
bugs: a wrong formula produces km-scale deviations. `checkInputFinite` rejects non-finite or
out-of-range input coordinates before the run.

CTest registers the fast correctness subset:

```bash
ctest --test-dir build/cpu-serial -R haversine-alpaka --output-on-failure
```

The registered test runs `--cities 100000 --repeat 4 --samples 2 --seed 7`. It passes when the
process exits zero (verification succeeded) and the `RESULT ... median_us=...` line is present.
The process exits non-zero on a verification failure, a non-finite result, an invalid argument or
an allocation failure.

## Build and run

CPU serial (source-tree alpaka, no OpenMP):

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target haversine-alpaka -j
./build/cpu-serial/bin/alpaka/haversine --cities 100000 --repeat 4 --samples 2 --seed 7
```

CPU with the OpenMP block executor (`OMP_NUM_THREADS=64`):

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target haversine-alpaka -j
```

CUDA (A30 / sm_80 by default):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target haversine-alpaka -j
./build/cuda/bin/alpaka/haversine --cities 2097152 --repeat 100 --samples 7
```

Use `-DCMAKE_CUDA_ARCHITECTURES=<n>` to target another GPU. Fast math is **OFF** by default; pass
`-DalpakaHeCBench_FAST_MATH=ON` (or `-Dalpaka_FAST_MATH=ON`) to enable device-side fast math.

## Memory footprint

`num_cities = 2^21` needs 42 MB for the location table and 6x that for the pair table, about
1.2 GB on the host plus device storage. Reduce `--cities` for small devices. The 1-based
`index_map` indices can exceed small `--cities` values; the port clamps the lookup index to
`num_cities - 1` for validation sizes. For the default `2^21` the clamp is inactive and the input
construction is identical to the reference.

## Upstream tooling

The vendored HeCBench tools discover this directory as benchmark `haversine`, model `alpaka`.
The CMake target builds `build/<preset>/bin/alpaka/haversine`.
