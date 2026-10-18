<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# gaussian (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`gaussian` benchmark. One C++20 translation unit solves a dense linear system `A x = b` with
Gaussian elimination without pivoting. It compiles unchanged for the host CPU and for a CUDA GPU;
there is no backend branch and no native CUDA/HIP/OpenMP kernel.

## Provenance

- HeCBench `gaussian` reference: commit `0a17ee47ca6072192101036d4bf292e6612f22e6`
  (`src/gaussian-cuda/gaussianElim.cu` and its OpenMP, HIP and SYCL twins).
- Original alpaka port: alpaka3 PR #48 head `0adabf114190` (`benchmark/gaussian`).
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the `extern/alpaka`
  submodule.
- The reference sources are not vendored. They are generated from the pinned HeCBench checkout, so
  the port ships no input data.

## What it computes

The benchmark generates a square Toeplitz matrix and a right-hand side of all ones:

```text
coe[i]  = 10 * exp(-0.01 * i)          // `-0.01f * i` is formed in float, then widened
A[i][j] = coe[size - 1 - i + j]
b[i]    = 1
```

For `t = 0 .. size - 2` one elimination step runs in two phases, matching the reference `fan1` and
`fan2` kernels:

1. the multiplier `m[row][t] = A[row][t] / A[t][t]` for every `row > t`,
2. the update `A[row][col] -= m[row][t] * A[t][col]` for `col > t` and
   `b[row] -= m[row][t] * b[t]`.

Back substitution then runs on the host, exactly as in the reference. The forward elimination runs on
the selected alpaka device; the host and CUDA builds share the same kernels and differ only in the
launch frame.

The reference writes the below-diagonal column `t` to zero. Back substitution never reads those
entries, so the port leaves them untouched. This removes a read/write race between the multiplier and
the column element without changing the solution. The multipliers live in a separate scratch matrix,
as in the reference.

## Command line

Named form:

```text
./gaussian --size N --repeat R --samples S [--verify-only] [--verbose]
```

- `--size` (`-s`) default `1024`. The upstream regression uses `-s 4096`.
- `--repeat` default `1`: forward eliminations per measured sample.
- `--samples` default `5`: independent measured samples.
- `--verify-only` runs the correctness check without the timing loop.
- `--verbose` prints the computed and oracle solution entries.

Positional form: `./gaussian <size> [repeat] [samples]`.

The native flags `-q` (quiet) and `-t` (timing) are accepted. The port never prints a matrix dump, so
`-q` has no visible effect. `-t` prints the native-compatible line
`Device offloading time <value> (us)`, where the value is the median forward-elimination time. The
`--size`/`-s` option covers the internally generated matrix used by the upstream regression. The
reference `-f <file>` input path is not ported.

Invalid, missing or out-of-range values print a diagnostic to stderr and exit non-zero.

## Test semantics

`src/helper.hpp` provides an independent oracle: a double-precision LU solve with partial pivoting.
It shares neither the no-pivot scheme nor the single precision of the benchmark, so a systematically
wrong elimination cannot pass. `verify()` checks

1. finiteness of every computed and every oracle entry,
2. `|x_i - gold_i| <= atol + rtol * |gold_i|` per entry with `atol = rtol = 1e-3`,
3. the relative residual `||A x - b||_2 / ||b||_2` recomputed in `double` from the computed solution
   against the original system, bounded by `1e-3`.

A failure prints the offending index and exits non-zero. The benchmark prints
`Verification PASSED` on success.

The CMake test uses the upstream `subset.json` arguments `-q -t -s 4096` and the upstream timing
regex. The test passes when the native-compatible timing line appears and the run exits successfully.

## Timing scope

The reported value is the forward-elimination time only, measured between two host synchronisations.
One warm-up elimination precedes the measured samples, and the reset copies are outside the timer.
This matches the reference `-t` scope, whose timer covers the `fan1`/`fan2` loop. The median of the
samples is reported.

## Build and run

CPU serial:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target gaussian-alpaka -j
./build/cpu-serial/bin/alpaka/gaussian -q -t -s 1024
```

CPU with the OpenMP executor:

```bash
OMP_NUM_THREADS=64 cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target gaussian-alpaka -j
OMP_NUM_THREADS=64 ./build/cpu/bin/alpaka/gaussian --size 1024 --samples 5
```

CUDA (A30 / sm_80):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target gaussian-alpaka -j
./build/cuda/bin/alpaka/gaussian -q -t -s 4096
```

Run the registered test:

```bash
ctest --test-dir build/cpu-serial -R gaussian-alpaka --output-on-failure
```

The `make` wrapper configures a `Release` build (`-O3`, `-march=native`), builds the `gaussian-alpaka`
target and copies `bin/alpaka/gaussian` to `./main` for the HeCBench tooling:

```bash
make -C src/gaussian-alpaka
python3 tools/hecbench -v run gaussian --model alpaka
```

`-v` is a global option and must precede the subcommand. Override the CUDA architecture with
`make CUDA_ARCH=<n>` (default `80`) and drop the host SIMD flag with `make HOST_CXX_FLAGS=`.

Fast math is OFF by default because the native HeCBench references use plain `-O3` without fast-math.
Pass `-DalpakaHeCBench_FAST_MATH=ON` or `-Dalpaka_FAST_MATH=ON` to match a fast-math reference.

This directory contains no measurement results, plots or result tables.
