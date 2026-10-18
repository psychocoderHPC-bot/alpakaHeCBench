<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# scan (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`scan` benchmark. One C++20 translation unit computes a chunk-local exclusive prefix sum for
integer inputs and is compiled unchanged for the host CPU and for a CUDA GPU. There is no backend
branch and no native CUDA/HIP/OpenMP kernel.

## Provenance

- HeCBench `scan` references: commit `0a17ee47ca6072192101036d4bf292e6612f22e6`
  (`src/scan-cuda/main.cu`, OpenMP twin `src/scan-omp/main.cpp`).
- Original alpaka port: alpaka3 PR #46 head `1127a7f7bab0` (`benchmark/scan`).
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the `extern/alpaka`
  submodule.

The reference sources stay outside this repository. The port follows their logic and does not
vendor them.

## Computed quantity

The input has `nelems = ceil(n / N) * N` elements and is split into chunks of `N` elements. Every
chunk is scanned independently:

```text
out[i]  = in[cN] + ... + in[i-1]   for cN < i < (c+1)N
out[cN] = 0
```

The running sum restarts at every chunk boundary, so a chunk never carries into the next one. This
matches the reference, which writes `t_out[0] = 0` and then `t_out[i] = t_out[i-1] + t_in[i-1]` per
chunk.

Two kernels cover the native variants:

| variant | shared memory |
| --- | --- |
| `scan` | `T[N]`, plain addressing |
| `scan_bcao` | `T[2*N]`, padded with `OFFSET(x) = x >> 5` |

Both variants run a grid-stride loop over chunks. The grid matches the reference's
`16 * multiProcessorCount`, cropped to the number of chunks.

Element types are `char`, `short`, `int` and `long`. Chunk sizes are 128, 256, 512, 1024 and 2048.
Input values come from `srand(123)` and `rand() % 5 + 1`, as in the reference.

## Command line

Reference-compatible positional form:

```text
./main <number of elements> <repeat>
```

Named options:

```text
--n N             number of input elements        (default: 67108864)
--chunk N         elements per block scan, 0 = all (default: 2048)
--repeat R        inner scans per sample           (default: 10)
--samples S       measured samples                 (default: 5)
--type T          char, short, int, long or all    (default: int)
--verify-only     run the checks without timing
```

`--chunk` accepts 0, 128, 256, 512, 1024 and 2048. Invalid values, unknown options, a missing
option value or an incomplete positional pair print an error and exit non-zero.

## Test semantics

`helper.hpp` reproduces the reference input generator and an independent sequential host exclusive
scan with the same chunk semantics. After a fresh device run the result is copied back and compared
element-wise against that host reference.

The comparison is exact. A chunk-local prefix sum over integers is an exact reduction, and the
reference itself compares with `memcmp` or `!=`. Narrow types may wrap; the host reference adds in
the same type, so the wrap-around matches bit for bit. Widening a tolerance would hide ordering
bugs.

The first mismatch prints the index, the expected value and the computed value. The process exits
non-zero when a check fails.

## Build and run

CPU serial:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target scan-alpaka -j
./build/cpu-serial/bin/alpaka/scan --n 1048576 --chunk 512 --repeat 4 --samples 2 --type int
```

OpenMP host executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target scan-alpaka -j
OMP_NUM_THREADS=16 ./build/cpu/bin/alpaka/scan --n 1048576 --type all --chunk 0
```

CUDA (A30 / sm_80):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target scan-alpaka -j
./build/cuda/bin/alpaka/scan --n 67108864 --chunk 2048 --repeat 10 --samples 5 --type int
```

## Test

```bash
ctest --test-dir build/cpu-serial -R scan-alpaka --output-on-failure
```

The CTest entry passes the benchmark arguments `--n 100000 --chunk 0 --repeat 2 --samples 2
--type int` and requires the final `scan verification PASSED` line. A validation failure prints
details to stdout and exits non-zero.
