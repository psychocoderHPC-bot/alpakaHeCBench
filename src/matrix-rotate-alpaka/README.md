<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# matrix-rotate (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`matrix-rotate` benchmark. One C++20 translation unit rotates a square `n x n` matrix of `float`
in place by 90 degrees clockwise and is compiled unchanged for the host CPU and for a CUDA GPU.
There is no backend branch and no native CUDA/HIP/OpenMP kernel.

## Provenance

- HeCBench `matrix-rotate` reference at commit
  `0a17ee47ca6072192101036d4bf292e6612f22e6`:
  `src/matrix-rotate-cuda/main.cu` and `src/matrix-rotate-omp/main.cpp`.
- Original alpaka port: alpaka3 PR #30 head `349f0fdf9456`.
- One-swap-per-thread optimization: alpaka3 PR #36 head `5a18f97bbef8`.
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the `extern/alpaka`
  submodule.

The implementation contains no parts of the HeCBench code base. `helper.hpp` and `misc.hpp` carry
a separate license header.

## Computation

The reference uses the HeCBench pyramid-ring scheme. For every ring `layer` the four corners are
swapped with one saved temporary while `i` walks from `first = layer` to `last = n - 1 - layer`.
The matrix starts as `m[i][j] = i * n + j` and is rotated `repeat` times.

The port removes the serial walk over each ring. It flattens the `(layer, i)` iteration space into
a single linear swap index, so one thread owns exactly one four-corner swap. The number of swaps is

```text
P = sum_{layer=0}^{n/2 - 1} (n - 1 - 2 * layer) = (n/2) * (n - n/2)
```

A thread recovers `(layer, i)` from its linear id in constant time with a closed-form estimate
followed by two exact correction loops. The correction loops keep the decode exact for every
floating-point rounding, so the permutation matches the reference. The launch shape depends on the
device properties, but correctness never depends on it.

## Command line

The primary interface matches the native binaries:

```text
./main <matrix size> <repeat>
```

The named form is also accepted:

```text
./main --size <n> --repeat <r>
```

`--size` and `--repeat` require positive integers. The defaults are `n = 5000` and `repeat = 100`,
which reproduce the HeCBench reference run. Invalid options, missing values and too many positional
arguments print an error to stderr and exit non-zero. `--help` prints the usage line and exits zero.

## Validation

`helper.hpp` provides an independent host reference. It fills the input exactly like the reference,
applies the ring swap sequentially in `double` without alpaka code, and compares the produced
`float` matrix entry by entry.

The comparison is bit-exact. Every entry is a non-negative integer below `n * n`, so it is exactly
representable as `float` for realistic sizes, and a rotation is a pure permutation. The tolerance is
therefore zero. Non-finite outputs are always reported and fail the check. The process exits
non-zero when validation fails.

## Build and run

Configure from the repository root with the source-tree alpaka preset, then build the target.

Serial CPU:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target matrix-rotate-alpaka -j
./build/cpu-serial/bin/alpaka/matrix-rotate 5000 100
```

CPU with the OpenMP block executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target matrix-rotate-alpaka -j
./build/cpu/bin/alpaka/matrix-rotate 5000 100
```

CUDA (A30 / `sm_80`):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target matrix-rotate-alpaka -j
./build/cuda/bin/alpaka/matrix-rotate 5000 100
```

The binary runs once per available backend. Set `CUDA_VISIBLE_DEVICES=<idx>` to pin a GPU.

## Test

The CTest entry uses the upstream benchmark arguments `5000 100` and matches the per-rotation
median printed by the port:

```bash
ctest --test-dir build/cpu-serial -R matrix-rotate-alpaka
```

The test fails when validation fails or the expected timing line is missing.

## Timing scope

The measured section brackets `repeat` consecutive kernel launches with one device
synchronization, matching the HeCBench reference. One full `repeat`-batch runs as warm-up first.
The reported metric is the median per-rotation time in microseconds, computed over seven measured
repetitions and divided by `repeat`. Allocation, host-device copies and the validation run stay
outside the timed section.
