<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# reverse (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`reverse` benchmark. One C++20 translation unit is compiled unchanged for the host CPU and for a
CUDA GPU. There is no backend branch and no native CUDA/HIP/OpenMP kernel.

## Computation

The benchmark reverses a dense `int` array of `length` elements in place:

```text
result[i] = input[length - 1 - i]
```

Each test case draws a count in `[100, 9999]` with `std::default_random_engine(123)` and
`std::uniform_int_distribution<int>(100, 9999)`, then launches that many back-to-back reverses. An
even count restores the identity sequence, an odd count leaves the array reversed. The reference
verifies against `gold_even[i] = i` or `gold_odd[i] = length - i - 1` with `memcmp` over the whole
buffer.

The port keeps the same draw order and the same even/odd verification. One logical worker owns each
mirrored swap `i <-> length - 1 - i` for `i < length / 2`, so no two workers read or write the same
slot and no block or grid synchronization is required. For odd `length` the middle element stays
untouched. This produces the same permutation as the reference kernel, which stages the array
through shared memory.

## Command line

The primary interface matches the native binary:

```text
./main <iterations>
```

The named form is also accepted:

```text
./main [--iterations <n>] [--length <n>] [--repeat <n>] [--all-backends]
```

The options also accept the `--option=value` spelling. Defaults are `iterations = 100`,
`length = 256` and `repeat = 5`. `length` must be positive and at most `4096`. All values must be
positive integers. Unknown options, missing option values, malformed numbers, non-positive values,
a length above the bound and extra positional arguments print a message to stderr and exit
non-zero. `--help` prints the usage line and exits zero.

Without `--all-backends` one invocation selects exactly one backend and one executor and reports
one timing line:

1. the first available non-host accelerator (CUDA, HIP or SYCL, including a SYCL CPU device),
2. otherwise `CpuOmpBlocks` on the host CPU,
3. otherwise `CpuSerial` on the host CPU.

`NumaCpu` is always skipped, so the host CPU is never measured twice. The campaign metadata in
`benchmarks.yaml` and `subset.json` uses the HeCBench reference count `100`; `--all-backends` is
opt-in and is not used by the campaign.

## Test semantics

`helper.hpp` builds the expected `gold_even` / `gold_odd` arrays exactly like the reference and
compares the device result element by element. It also reproduces the reference `memcmp` over the
raw bytes as an independent cross-check. The comparison is exact: a reverse is a byte-preserving
permutation of `int` values, so no floating-point tolerance applies. Non-finite values are rejected
even though the data is integral. A failure prints `Reverse verification FAILED` and returns
non-zero; success prints `PASS`.

A warm-up pass validates every test case. The measured passes skip the copy-back and the
verification, so only the per-case launch loop is timed. The reported value is the median over the
`repeat` measured passes. Only the back-to-back kernel enqueues between two device synchronizations
are inside the timed region; allocation, host/device copies and validation stay outside.

The median is the benchmark metric of this port, not the single ORNL kernel time. ORNL launches the
whole test-case loop once and prints its total; this port discards a validated warm-up and reports
the median of `repeat` measured totals. Campaign results are therefore comparable across alpaka
backends, but the absolute value is not byte-for-byte the ORNL metric. Per-repetition samples go to
stderr, so stdout carries exactly one timing line per selected backend.

## Provenance

- HeCBench reference: commit
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/tree/0a17ee47ca6072192101036d4bf292e6612f22e6),
  `src/reverse-cuda/main.cu`, `src/reverse-omp/main.cpp`, `src/reverse-hip/main.cu` and
  `src/reverse-sycl/main.cpp`.
- Original alpaka port: alpaka3 PR #33, head `715a84360384656fc34318069f76a818735e1df4`,
  `benchmark/reverse`.
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the
  `extern/alpaka` submodule.

The HeCBench reference sources are not vendored here. HeCBench is Copyright (c) 2020 Argonne
National Laboratory (Zheming Jin) and Copyright (c) 2020- Oak Ridge National Laboratory (Zheming
Jin), distributed under the BSD 3-Clause License. The project root `LICENSE` covers this port.

## Build and run

Serial CPU, Release, from the repository root:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target reverse-alpaka -j
./build/cpu-serial/bin/alpaka/reverse 100
```

CPU with the OpenMP block executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target reverse-alpaka -j
OMP_NUM_THREADS=64 ./build/cpu/bin/alpaka/reverse 100
```

CUDA (A30, `sm_80`):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target reverse-alpaka -j
./build/cuda/bin/alpaka/reverse 100
```

The target name is `reverse-alpaka` and the executable is written to
`build/<preset>/bin/alpaka/reverse`. By default the benchmark runs one selected backend and prints
one timing line. Set `CUDA_VISIBLE_DEVICES=<idx>` to pin a GPU, or pass `--all-backends` to run
every enabled non-NUMA backend for diagnostics.

## Test

```bash
ctest --test-dir build/cpu-serial -R reverse-alpaka --output-on-failure
```

CTest runs the native positional form `reverse-alpaka 4` and matches the median timing line. It
also sets `FAIL_REGULAR_EXPRESSION "FAIL"`: CTest treats a matching `PASS_REGULAR_EXPRESSION` as a
pass even when the process exits non-zero, so the failure marker is what makes a validation failure
fail the test. The test also fails when the expected timing line is missing.
