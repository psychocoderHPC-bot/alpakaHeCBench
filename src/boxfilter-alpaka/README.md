<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# boxfilter (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of
the HeCBench `boxfilter` benchmark. One C++20 translation unit applies a
separable 2D box blur to a packed RGBA image and compiles unchanged for the host
CPU and for a CUDA GPU. There is no backend branch and no native
CUDA/HIP/OpenMP kernel.

## Computation

For an image of `width x height` pixels and radius `r`, the port runs two passes.

Row pass: every output pixel receives the truncated average of its horizontal
window on the same row. Pixels outside `[0, width)` contribute zero, matching
the reference shared-memory padding.

```text
tmp[y][x] = floor(sum(input[y][xx] for xx in [max(0,x-r), min(width-1,x+r)]) / (2r+1))
```

Column pass: every output pixel receives the truncated average of the clamped
vertical window of the intermediate image. The clamp reproduces the reference
replicate boundary for `height >= 2r+1`.

```text
out[y][x] = floor(sum(tmp[yy][x] for yy in [max(0,y-r), min(height-1,y+r)]) / (2r+1))
```

Each pixel is a 32-bit packed RGBA word; the four channels are summed and
quantized independently. The kernel datapath is integer, so results are exact.

## Parameters and test semantics

```text
./boxfilter [<image>] [<repeat>] [--width <n>] [--height <n>] [--radius <n>]
            [--repeat <n>] [--samples <n>] [--verify-only]
```

Defaults are `width = 1024`, `height = 1024`, `radius = 10`, `repeat = 100` and
`samples = 5`. The reference reads an external PPM image and takes
`<PPM image> <repeat>` positionally. This port generates a deterministic RGBA
image internally, so the positional image path is accepted and ignored for CLI
compatibility. The positional repeat form matches the reference. `--repeat` and
the positional repeat are mutually exclusive. Invalid values, missing values,
unknown options and out-of-range parameters exit non-zero.

`repeat` counts back-to-back filter passes inside one measured sample. One
warm-up pass is discarded. The program prints the upstream-compatible field
`Average kernel execution time <value> us` and the full sample statistics.
`--verify-only` skips the timing loop and runs only the correctness checks.

Validation is independent of the kernel and fails the process with a non-zero
exit code:

- `boxFilterGold` recomputes both passes on the host in 64-bit integer
  arithmetic from the same generated input.
- `verify` compares the accelerator output with that reference exactly
  (tolerance 0) over rows `[radius, height-radius)`, the region the HeCBench
  harness checks.
- `checkFinite` re-derives row sums in double and rejects non-finite values.
  Generated inputs have zero alpha, so a non-zero output alpha fails.
- `height <= 2*radius` is rejected because the compared region would be empty.

The registered CTest case runs the positional repeat form with `repeat = 4`,
`samples = 2` and the default image size. It passes only when the upstream
timing line appears and verification succeeds.

## Provenance

- HeCBench reference pinned at
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/tree/0a17ee47ca6072192101036d4bf292e6612f22e6),
  `src/boxfilter-cuda`, `src/boxfilter-omp` and `src/boxfilter-sycl`. The
  upstream sources are kept outside this repository and are not vendored.
- Original alpaka port: alpaka3 PR #38 head
  [`c0339d49a15c`](https://github.com/psychocoderHPC-bot/alpaka3/pull/38),
  `benchmark/boxfilter`.
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as
  the `extern/alpaka` submodule.

The reference CUDA/OpenMP/host sources carry NVIDIA's BSD-3-Clause header
(Copyright 1993-2010 NVIDIA Corporation). Upstream HeCBench is BSD-3-Clause.
This port and its helper files are BSD-3-Clause. The README is CC0-1.0.

## Input data

Nothing from the ORNL input tree is vendored. The reference image
`src/boxfilter-sycl/data/lenaRGB.ppm` can be obtained from
[ORNL/HeCBench](https://github.com/ORNL/HeCBench) at the pinned commit. This
port does not read it: it generates a deterministic 8-bit RGBA image (zero
alpha) so the benchmark is self-contained.

## Build, run and test

CPU serial, Release:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target boxfilter-alpaka -j
./build/cpu-serial/bin/alpaka/boxfilter --width 1920 --height 1080 --radius 10 --repeat 20 --samples 5
ctest --test-dir build/cpu-serial -R boxfilter-alpaka --output-on-failure
```

CPU with the OpenMP block executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target boxfilter-alpaka -j
./build/cpu/bin/alpaka/boxfilter 4
```

CUDA (architecture set by the preset, default `sm_80`):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target boxfilter-alpaka -j
./build/cuda/bin/alpaka/boxfilter --width 1920 --height 1080 --radius 10 --repeat 100 --samples 5
```

The executable runs every enabled backend and executor and reports each one
separately. The vendored HeCBench tooling can run the port with
`python3 tools/hecbench -v run boxfilter --model alpaka`.

This repository ships no measurement data, plots, result tables or campaign
scripts.
