<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# axpby (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of
the HeCBench `axpby` benchmark. One C++20 translation unit computes the fused
elementwise update `out = a * x + b * y` over one or more independent dense
tensors and is compiled unchanged for the host CPU and for a CUDA GPU. There is
no backend branch and no native CUDA/HIP/OpenMP kernel.

## Computation

For every tensor `t` and element `i` the output is

```text
out_t[i] = a * static_cast<float>(x_t[i]) + b * static_cast<float>(y_t[i])
```

with the HeCBench defaults `a = 1` and `b = 1`. Each tensor has its own length.
The host builds the same flat `(tensor, chunk)` block list as the reference
`multi_tensor_apply` launcher, so one kernel launch covers every tensor: each
block owns one chunk of one tensor and threads within the block stride over that
chunk. Chunk size defaults to `512`, matching the reference `BLOCK_SIZE`.

Input generation follows the reference draw order: `srand(123)`, then per tensor
a length from `rand() % (1024*1024) + 1024`, then the `x` values, then the `y`
values. `--fixed-length` forces a common length for deterministic testing.

## Command line

```text
./axpby [--tensors <n>] [--chunk-size <n>] [--repeat <n>] [--fixed-length <n>] [<tensors>]
```

Defaults are `--tensors 1000`, `--chunk-size 512`, `--repeat 5` and
`--fixed-length 0`. The bare positional `<tensors>` form matches the native
HeCBench binary. `-h`/`--help` prints the usage text. Invalid or missing option
values and out-of-range parameters exit non-zero.

## What the test validates

The host-side `axpby::verify()` recomputes every element in double precision from
the stored inputs and compares it with the accelerator output, tensor by tensor.
The kernel uses single precision; the inputs are non-negative integers below
2^20 and `a = b = 1`, so the default `x + y` is exactly representable as
`float`. The check uses `atol = 1e-3`, the same threshold as the HeCBench
`fabsf(...) > 1e-3` check, plus a small relative term for compiler contraction
differences. Non-finite output fails explicitly, and the process exits non-zero
on any mismatch.

The registered CTest case runs four tensors of odd length `4093` with chunk size
`512` and two measured repetitions. It passes only when the timing line appears
and the verification succeeds.

## Provenance

- HeCBench reference pinned at commit
  `0a17ee47ca6072192101036d4bf292e6612f22e6`:
  `src/axpby-cuda/main.cu`, `src/axpby-cuda/multi_tensor_apply.cuh` and
  `src/axpby-omp/main.cpp` (kept outside this repository, not vendored).
- Original alpaka port: alpaka3 PR #27 head `a60ba76cb8a1`,
  `benchmark/axpby`.
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as
  the `extern/alpaka` submodule.

The port and the helper files are new work under BSD-3-Clause. They contain no
HeCBench source.

## Build, run and test

CPU serial:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target axpby-alpaka -j
./build/cpu-serial/bin/alpaka/axpby --tensors 50 --chunk-size 512 --repeat 5
ctest --test-dir build/cpu-serial -R axpby-alpaka --output-on-failure
```

CPU with the OpenMP block executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target axpby-alpaka -j
./build/cpu/bin/alpaka/axpby 50
```

CUDA (architecture set by the preset, default `sm_80`):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target axpby-alpaka -j
./build/cuda/bin/alpaka/axpby --tensors 1000 --chunk-size 512 --repeat 5
```

Timing brackets only the kernel enqueue and the device synchronization, matching
the reference, which wraps the launch with `cudaDeviceSynchronize()`. One warm-up
launch is discarded and the median of the measured launches is printed. Fast math
is off by default because the native HeCBench references use plain `-O3`.
