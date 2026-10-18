<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# softmax (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`softmax` benchmark. One C++20 translation unit computes a numerically stable row-wise softmax over
a dense `(numSlice, sliceSize)` tensor and is compiled unchanged for the host CPU and for a CUDA
GPU. There is no backend branch and no native CUDA/HIP/OpenMP kernel.

## Provenance

- HeCBench `softmax` reference: commit `0a17ee47ca6072192101036d4bf292e6612f22e6`
  (`src/softmax-cuda/main.cu`, `src/softmax-omp/main.cpp`).
- Original alpaka port: alpaka3 PR #55 head `19f5dc918d01` (`benchmark/softmax`).
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the `extern/alpaka`
  submodule.

The port re-derives the standard softmax formulation; it does not vendor HeCBench code. HeCBench is
distributed under BSD-3-Clause. The port and the host helpers use BSD-3-Clause as well.

## Computed quantities

For every row (slice) of the input tensor:

```text
m   = max_j x[j]
e_j = exp(x[j] - m)
s   = sum_j e_j
y_j = e_j / s
```

One worker owns one complete row. The worker walks the row three times: maximum, exponential sum,
normalization. This is the structure of the HeCBench measured variant `softMax` (implementation 0,
one thread per slice). Input generation matches the reference: `std::srand(2)` followed by
`rand() % 13` per element.

The port produces the same values as the reference variant 1 (`softMax2`, warp-cooperative) but
implements only the measured variant 0.

## Command line

Native positional form, byte-compatible with the HeCBench binaries:

```text
./softmax <number of slices> <slice size> <implementation> <repeat>
```

The third argument selects the reference implementation and is accepted but ignored; the port
measures variant 0 only. Named options are also accepted:

```text
./softmax [--size <rows>] [--slice-size <n>] [--repeat <n>] [--dtype <float|double|both>]
```

Defaults are `--size 100000 --slice-size 784 --repeat 100 --dtype float`, the upstream test recipe
(`100000 784 0 100`). Invalid, missing or non-positive values print an error to stderr and exit
non-zero.

## Output and test semantics

The benchmark prints the native timing line to stdout:

```text
Average kernel execution time: <value> (ms)
```

The timed region covers the whole enqueue loop plus one device synchronization; allocation,
host/device copies and reference computation stay outside. The value is the mean kernel time over
`--repeat` iterations, matching `src/softmax-cuda/main.cu`.

Validation compares the produced output with an independent double-precision host reference derived
from the stored inputs. FP32 uses an absolute tolerance of `1e-3` (the HeCBench check) plus
`1e-4 * |ref|`; FP64 uses `1e-9`. Non-finite samples fail explicitly. The run prints
`Verification PASSED` or `Verification FAILED` and exits non-zero on failure.

The CTest entry runs the upstream arguments `100000 784 0 100` and passes when the native timing line
appears within the 300 s timeout.

## Build and run

Configure and build the target with a repository preset:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target softmax-alpaka -j
```

The binary is written to `build/cpu-serial/bin/alpaka/softmax`. Run the upstream recipe or a short
smoke test:

```bash
./build/cpu-serial/bin/alpaka/softmax 100000 784 0 100
./build/cpu-serial/bin/alpaka/softmax --size 4096 --slice-size 128 --repeat 2
```

OpenMP CPU build:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target softmax-alpaka -j
```

CUDA build (A30 / sm_80):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target softmax-alpaka -j
./build/cuda/bin/alpaka/softmax 100000 784 0 100
```

Fast math is OFF by default because the native HeCBench softmax references use plain `-O3` without
fast-math. Pass `-DalpakaHeCBench_FAST_MATH=ON` or `-Dalpaka_FAST_MATH=ON` at configure time to
enable device-side fast-math.

## Test

```bash
ctest --test-dir build/cpu-serial -R softmax-alpaka --output-on-failure
```

## Limitations

The reference's warp-cooperative `softMax2` variant is not ported. The benchmark measures only the
native variant 0 with one worker per row.
