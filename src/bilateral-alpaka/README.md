<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# bilateral (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`bilateral` benchmark. One C++20 translation unit applies a bilateral image filter to a
single-channel `float` image and is compiled unchanged for the host CPU and for a CUDA GPU. There is
no backend branch and no native CUDA/HIP/OpenMP kernel.

## Provenance

- HeCBench `bilateral` reference: commit `0a17ee47ca6072192101036d4bf292e6612f22e6`
  (`src/bilateral-cuda/main.cu`, `src/bilateral-cuda/reference.h`, `src/bilateral-omp/main.cpp`).
  The reference sources are not vendored in this repository.
- Original alpaka port: PR #57 head `4934d9620e49` (`benchmark/bilateral/src/main_alpaka.cpp`,
  `helper.hpp`, `misc.hpp`).
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the `extern/alpaka`
  submodule.

## Computation

For every output pixel `(idx, idy)` the filter accumulates a normalised weighted average over a
`(2R+1) x (2R+1)` window centred on the pixel:

```text
range   = -(I - I_w)^2 / (2 * variance_I)
spatial = -((idk - idx)^2 + (idl - idy)^2) / (2 * variance_spatial)
weight  = a_square * exp(spatial + range),   a_square = 0.5f / (variance_I * pi)
out     = sum(I_w * weight) / sum(weight)
```

The window uses the reference's mirror-edge index folding. The pinned reference runs radii
`R = 3, 6, 9` on a single-channel image. The port keeps the reference arithmetic and traversal order
in single precision, including `exp` and the accumulation order. `R` is a compile-time parameter for
the three reference radii; a runtime-radius kernel exists only so `--radius` accepts other legal
values for quick checks.

Input reproduction matches the reference sequence `srand(123); rand() % 256`. `helper.hpp`
reimplements glibc's `random()` (`TYPE_3`, degree 31, separation 3) so the input does not depend on
the host libc.

## Command line

The native positional form is accepted:

```text
./bilateral <width> <height> <intensity> <spatial> <repeat>
```

The same values are available as named options:

- `--width` (default `2960`)
- `--height` (default `1440`)
- `--intensity` (default `0.5`, range-kernel variance)
- `--spatial` (default `0.5`, spatial-kernel variance)
- `--repeat` (default `1000`, filter passes per measured sample)
- `--samples` (default `5`, independent measured samples)
- `--radius` (default `-1`, which runs the reference set `{3,6,9}`)
- `--verify-only` (skip timing, run the correctness check only)

Named options may appear before or after the positional values. Invalid values, out-of-range sizes,
and a radius that does not fit the image are rejected with a diagnostic and a non-zero exit code.

## Self-validation and test semantics

The port computes an independent double-precision host reference from the same input and compares
the accelerator output against it after a fresh run. Every output value is checked for NaN/Inf, and a
zero or NaN normalisation in the host reference fails the run. The comparison tolerance is `1e-3`,
the pinned harness's own threshold for the identical computation. A non-zero exit code signals
failure, so the binary itself is a validation test.

CTest registers `bilateral-alpaka` with `--width 128 --height 96 --verify-only`. This runs all three
reference radii and the host reference on a small image, which keeps the test fast on a CPU backend.
The test passes when the process exits successfully and prints `bilateral verification PASSED`. The
default measurement configuration is the reference image size and repeat count; it is not what the
registered test runs.

## Build and run

CPU (source-tree alpaka, serial executor):

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target bilateral-alpaka -j
./build/cpu-serial/bin/alpaka/bilateral --width 128 --height 96 --verify-only
```

CPU with the OpenMP block executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target bilateral-alpaka -j
OMP_NUM_THREADS=$(nproc) ./build/cpu/bin/alpaka/bilateral 2960 1440 0.5 0.5 1000
```

CUDA (A30 / sm_80):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target bilateral-alpaka -j
./build/cuda/bin/alpaka/bilateral 2960 1440 0.5 0.5 1000
```

Run the registered test:

```bash
ctest --test-dir build/cpu-serial -R bilateral-alpaka --output-on-failure
```

`--preset cpu` and `--preset cuda` configure Release builds. The CUDA preset defaults to
`CMAKE_CUDA_ARCHITECTURES=80`; override it on the configure line for another GPU. Fast math is off by
default because the native HeCBench reference is built without it. Pass
`-DalpakaHeCBench_FAST_MATH=ON` to enable device-side fast math.

The binary writes one `RESULT` line per radius with the median, minimum, maximum, and relative
spread of the measured samples, followed by `bilateral verification PASSED` when validation
succeeds. With `--verify-only`, the timing lines are omitted.
