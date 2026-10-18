<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# stddev (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`stddev` benchmark. One C++20 translation unit drives the host CPU backend and every device backend;
there is no native CUDA/HIP/OpenMP kernel and no backend branch.

## Provenance

- HeCBench `stddev` reference: commit `0a17ee47ca6072192101036d4bf292e6612f22e6`
  (`src/stddev-cuda/main.cu`, `src/stddev-cuda/reference.h`, `src/stddev-omp/main.cpp`). The CUDA
  reference is Apache-2.0, "Copyright (c) 2018-2020, NVIDIA CORPORATION", derived from NVIDIA RAPIDS
  `raft`. Upstream HeCBench is BSD-3-Clause.
- Original alpaka port: alpaka3 PR #40 head `5fa1581aa073` (`benchmark/stddev`).
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the `extern/alpaka`
  submodule.
- The HeCBench sources are kept outside this repository and are not vendored.

## Computed quantity

For every column `c` of an `N x D` row-major `float` matrix the pinned reference computes

```text
std[c] = sqrt( sum_{r=0}^{N-1} data[r*D + c]^2 / sampleSize )
```

with `sampleSize = N - 1`. Despite the name this is a root-mean-square sum-of-squares reduction, not
a mean-centred standard deviation; the reference always passes `sample = true`.

The port keeps the `float` value type, the two-stage pipeline and the atomic accumulation. Pass 1
(`SumOfProductsKernel`) maps a flat worker index `i = s*D + c` to consecutive columns and a row chunk,
accumulates four independent partial sums and adds the result into `std[c]` with a device-scope
atomic. Pass 2 (`NormalizeKernel`) multiplies by `1/(N-1)` and takes the square root. Because pass 1
uses atomics, the output is zeroed with `onHost::memset` before every launch, matching the reference's
`cudaMemset`. The row split targets about `1048576` workers and keeps at least eight rows per worker.

## Command line

Positional form, matching the HeCBench tooling:

```text
./stddev <D> <N> <repeat>
```

Named form:

```text
./stddev --D <cols> --N <rows> --repeat <r> --samples <s>
```

`--D` defaults to `65536`, `--N` to `16384`, `--repeat` to `100` and `--samples` to `5`.
`N` must be at least 2. `--name=value` is accepted as well as `--name value`. Invalid or unknown
arguments print an error and exit non-zero.

## Validation and output

`helper.hpp` regenerates the input with the reference's `srand(123)` / `rand()/(float)RAND_MAX`
sequence and evaluates the same formula independently in `double`, streaming the matrix in linear
order. `verify()` rejects NaN/Inf and compares with `|got - ref| <= 1e-3 + 1e-3 * |ref|`. The absolute
term is the reference's own tolerance; the relative term scales it for large values. The benchmark
prints one median timing line and then `PASS` or `FAIL`, and exits non-zero on failure.

The timed scope is `repeat` back-to-back `memset + sum-of-squares + normalise` pipelines between two
synchronisations, divided by `repeat`. Kernel time only; the device-to-host copy and the host oracle
are outside the timed region. One warm-up pipeline precedes the measured samples.

## Build and run

CPU serial (the local validation configuration):

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target stddev-alpaka -j
./build/cpu-serial/bin/alpaka/stddev 320 97 2
```

CPU with the OpenMP executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target stddev-alpaka -j
OMP_NUM_THREADS=64 ./build/cpu/bin/alpaka/stddev 4096 4096 50
```

CUDA (A30 / sm_80):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target stddev-alpaka -j
./build/cuda/bin/alpaka/stddev 65536 16384 100
```

## Test

```bash
ctest --test-dir build/cpu-serial -R stddev-alpaka --output-on-failure
```

CTest registers a fast correctness subset (`--D 320 --N 97 --repeat 2 --samples 2`) rather than the
upstream `65536 16384 100` shape, which is too slow for a practical test on the serial CPU executor.
The upstream arguments and regex stay in `benchmarks.yaml` and `src/scripts/benchmarks/subset.json`
for the HeCBench metadata tooling.

Fast math is OFF by default, matching the plain `-O3` native references. Pass
`-DalpakaHeCBench_FAST_MATH=ON` (or `-Dalpaka_FAST_MATH=ON`) at configure time for device-side
fast-math flags.
