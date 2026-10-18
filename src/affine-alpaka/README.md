<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# affine

Single-source alpaka port of the HeCBench `affine` benchmark. It applies a fixed
2D affine image transform to a 16-bit grayscale image. The same C++20 source is
compiled for the host CPU and for CUDA; there is no native CUDA/HIP/OpenMP kernel
and no backend branch.

## What it computes

For every output pixel `(x, y)` the transform maps back to a source coordinate
through the inverse of the fixed forward matrix:

```
affine = [[0.5*cos(30 deg), 0.5*sin(0 deg)],
          [0.5*sin(30 deg), 0.5*cos(0 deg)]]   beta = [0, 0]
```

The result is a bilinear sample of the four surrounding source pixels, truncated
to `unsigned short`. A sample that lands on the last row or column uses the
nearest edge pixel; samples outside the image are filled with `WHITE = 1`. All
arithmetic is single precision (`float`), matching the pinned reference.

The forward coefficients are constant, so the port computes the inverse matrix
once on the host and passes it to the kernel. The reference recomputes the same
single-precision constants per pixel, and its compiler folds them; hoisting them
does not change the arithmetic. Parallelism uses `onAcc::makeIdxMap` over a 2D
`IdxRange{width, height}`, so one kernel body covers every enabled backend. The
64x4 frame extent is only a launch-shape hint.

The reference reads an external RAW image. This port is self-contained: it
generates a deterministic 16-bit image in `[0, 4095]` with a xorshift64*
generator in `helper.hpp`.

## Command line

```
affine [--width W] [--height H] [--repeat R] [--samples S] [--verify-only]
affine [W [H [R [S]]]]
```

- `--width`, `--height`: image size, default `512` each (the reference constants).
- `--repeat`: transform passes per measured sample, default `100`.
- `--samples`: independent measured samples, default `5`.
- `--verify-only`: run validation only, no timing loop.
- `-h`, `--help`: print usage.

Options accept both `--name value` and `--name=value`. The positional form takes
width, height, repeat and samples in that order. Zero sizes, non-positive repeat
or sample counts, unknown options and malformed values fail with a nonzero exit
code. The transform itself is fixed by the reference and is not configurable.

## Validation and test semantics

Two host references are recomputed from the same generated input:

- `affineReference` translates the pinned single-precision reference algorithm.
- `affineReferenceDouble` evaluates the same geometry in double precision as an
  independent oracle.

The single-precision reference must agree with the double-precision oracle
within one grey level. The accelerator output is compared against the
single-precision reference with the same tolerance. One grey level is the
justified bound for a truncated bilinear sample of a single-precision transform;
a larger difference is a defect. `checkFinite` additionally checks input,
transform intermediates and output for finite values and the valid 16-bit range.

The registered CTest case runs a fast correctness subset:

```
ctest --test-dir build/cpu-serial -R affine-alpaka --output-on-failure
```

It runs `127x129`, `--repeat 4`, `--samples 2`, so it exercises an irregular
size as well as the validation path. CTest passes on the `RESULT` line, which the
benchmark prints after the timing samples. The same arguments and regex are
registered in `benchmarks.yaml` and `src/scripts/benchmarks/subset.json` for the
upstream tooling.

The timing scope is one measured sample per `repeat` back-to-back enqueues
between synchronizations, with one warm-up pass first. `RESULT` reports median,
minimum, maximum and relative spread of the samples. This repository does not
ship measured results or reference ratios.

## Build and run

CPU serial, from the repository root:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target affine-alpaka -j
./build/cpu-serial/bin/alpaka/affine --repeat 100 --samples 5
```

The OpenMP block executor is available through the `cpu` preset:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target affine-alpaka -j
OMP_NUM_THREADS=14 ./build/cpu/bin/alpaka/affine --repeat 100 --samples 5
```

CUDA, with the target architecture matching the GPU:

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target affine-alpaka -j
./build/cuda/bin/alpaka/affine --repeat 100 --samples 5
```

`-Dalpaka_COMPILE_PEDANTIC=ON` is recommended for portability checks. The CPU
presets use `-march=native`; override `CMAKE_CXX_FLAGS` for redistributable
builds.

## Provenance

- Reference: [ORNL/HeCBench](https://github.com/ORNL/HeCBench) at commit
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/commit/0a17ee47ca6072192101036d4bf292e6612f22e6).
  - CUDA, HIP and SYCL share `src/affine-cuda/kernel.h` and
    `src/affine-cuda/reference.h` behind `src/affine-cuda/main.cu`.
  - OpenMP carries the same arithmetic inline in `src/affine-omp/main.cpp`.
  - HeCBench entry: "affine (opencl): Affine transformation", originally from
    the Xilinx SDAccel examples, `vision/affine`.
- The reference sources are not vendored here. This clean-room alpaka
  reimplementation is `BSD-3-Clause`; `helper.hpp` carries the host reference,
  input generator and checks, and `main.cpp` carries the kernel and driver.
  `misc.hpp` contains only the command line parser.

The upstream license text is BSD-3-Clause with the Xilinx copyright notice in
the reference source headers. See the repository `LICENSES/` and `REUSE.toml`
for the file-level mapping.
