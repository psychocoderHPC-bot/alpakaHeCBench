<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# popcount (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`popcount` benchmark. One C++20 translation unit is compiled unchanged for the host CPU and for a
CUDA GPU.

## Computation

For every element of a 64 bit integer array the kernel returns the number of set bits:

```text
output[i] = popcount(input[i])     // 0 .. 64
```

The kernel calls `alpaka::popcount`, which lowers to `std::popcount` on the host and to the native
`__popcll` on NVIDIA hardware. This matches the hardware-intrinsic variant (`pc6`) of the HeCBench
reference. The SWAR masks, the measured Kernighan loop, the lookup table and the
`__builtin_popcountll` variant of the reference are not reproduced; Kernighan's loop is retained
only as the host reference used by validation.

Data generation matches the reference bit for bit: `srand(2)` followed by two `rand()` draws per
element, with the first draw shifted into the high 32 bits:

```text
input[i] = ((uint64_t)rand() << 32) | rand()
```

## Command line

The parser in `misc.hpp` uses only the C++ standard library, so the benchmark does not depend on
Catch2. It accepts the native positional form and the option form:

```text
./main [<count> [<repeat>]] [--size <count>] [--repeat <n>]
```

`--size` and `--repeat` also accept the `--option=value` spelling. The defaults are
`count = 16777216` and `repeat = 100`. Both values must be positive integers. Unknown options,
missing option values, malformed numbers, non-positive values and extra positional arguments print
a message to stderr and exit non-zero.

One warm-up enqueue runs first. The reported value is the median of the `repeat` measured
iterations. Only the kernel enqueue and the device wait are timed; allocation, host/device copies
and validation are excluded.

## Test semantics

`helper.hpp` contains the host reference `popcountReference`, the exact Kernighan loop of the
HeCBench reference `popcount_ref`. The device output is compared bit-exact against it on the host,
which is sufficient because popcount returns an exact integer. `checkRange` in `main.cpp`
independently rejects any host result outside `[0, 64]` before the reference comparison. A failure
prints `Popcount verification FAILED` and returns non-zero; success prints
`Popcount verification PASSED`.

CTest runs the binary as `popcount-alpaka 4096 2` and matches the timing line
`Popcount kernel runtime median: <value> us`.

## Provenance

- HeCBench reference: commit
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/tree/0a17ee47ca6072192101036d4bf292e6612f22e6),
  `src/popcount-cuda/main.cu` and `src/popcount-omp/main.cpp`.
- Original alpaka port: alpaka3 PR #32, head `eeeda9e0fa465cc95f55540e95154d884f55479a`,
  `benchmark/popcount`.
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the
  `extern/alpaka` submodule.

The HeCBench reference sources are not vendored here. HeCBench is Copyright (c) 2020 Argonne
National Laboratory (Zheming Jin) and Copyright (c) 2020- Oak Ridge National Laboratory (Zheming
Jin), distributed under the BSD 3-Clause License. The project root `LICENSE` covers this port.

## Build and run

Serial CPU, Release, from the repository root:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target popcount-alpaka -j
./build/cpu-serial/bin/alpaka/popcount 16777216 5
```

CPU with the OpenMP block executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target popcount-alpaka -j
OMP_NUM_THREADS=64 ./build/cpu/bin/alpaka/popcount 16777216 5
```

CUDA (A30, `sm_80`):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target popcount-alpaka -j
./build/cuda/bin/alpaka/popcount 16777216 5
```

The target name is `popcount-alpaka` and the executable is written to
`build/<preset>/bin/alpaka/popcount`. The benchmark runs every enabled backend and executor and
prints one timing block per device.

## Test

```bash
ctest --test-dir build/cpu-serial -R popcount-alpaka --output-on-failure
```

If alpaka is installed instead of using the `extern/alpaka` submodule, configure with
`-DalpakaHeCBench_USE_SOURCE_TREE=OFF`.
