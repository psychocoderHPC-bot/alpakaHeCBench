<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# medianfilter (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of
the HeCBench `medianfilter` benchmark. One C++20 translation unit applies a 2D
3x3 median filter to an RGBA image and is compiled unchanged for the host CPU and
for a CUDA GPU. There is no backend branch and no native CUDA/HIP/OpenMP kernel.

## What it computes

For every output pixel and every one of the three colour channels, the filter
returns the median of the 3x3 neighbourhood centred on the pixel. The alpha
channel is dropped; the reference packs only R, G and B into one 32-bit word at
bits 0, 8 and 16. Pixels outside the image contribute zero (zero padding at the
border).

The median uses the reference's fixed 8-iteration integer binary search over the
256 grey levels, starting from the estimate 128 and the bound `[0, 255]`. The
binary search is not always the exact sorted median: for an all-zero window it
converges to 1 while the sorted median is 0, a known property of the NVIDIA SDK
search. The port reproduces the reference search exactly.

The reference device kernel stages a 16x4 tile with a one-pixel apron in shared
memory and runs the binary search over the staged window. This port keeps the
same algorithm, window and border semantics. Instead of shared-memory tiling it
samples the 3x3 window once per pixel into registers and runs the identical
binary search over those values. The result is bit-for-bit the same as the
reference computation; only the data staging differs, so the kernel stays
portable across every alpaka backend.

## Parameters

- Positional HeCBench form: `medianfilter [<image>] <repeat>`.
  The image path is accepted and ignored because the port generates a
  deterministic input; `<repeat>` matches the reference positional argument.
- Named form: `medianfilter --width W --height H --repeat R --samples S
  [--verify-only]`.
- `--width` / `--height` default to `1920` / `1080`, matching the reference input
  size. Any size >= 1 is accepted.
- `--repeat` defaults to `100`: filter passes per measured sample.
- `--samples` defaults to `5`: independent measured samples.
- `--verify-only` runs the correctness check without the timing loop.
- `-h` / `--help` prints the usage.

The pinned reference is a fixed 3x3 filter, so there is no window-size option.

## Test semantics

Two trusted host references are recomputed from the same deterministic input:

- `medianGold` (`helper.hpp`) reproduces the reference binary search bit-for-bit.
  The accelerator result is compared against it with tolerance 0 (`verify`). Any
  mismatch prints the pixel coordinates and both packed values and returns a
  nonzero exit code.
- `medianSorted` computes the exact sorted 3x3 median, an independent oracle.
  `verifyReferenceAgainstSorted` asserts that the binary search and the sorted
  median differ by at most one grey level per channel, and only where the exact
  median is zero and the search returns one.

`checkFinite` re-derives every 3x3 window in double, asserts each sample is finite
and inside `[0, 255]`, and asserts that the output alpha channel is zero. The
integer datapath cannot produce NaN or Inf; this check makes that explicit. A
nonzero exit code signals failure.

## Input data

The reference reads an external PPM image (`SierrasRGB.ppm`, 1920x1080). This
port is self-contained: `generateImage` (`helper.hpp`) produces a deterministic
8-bit RGBA image with a xorshift64* generator, with zero alpha, matching
`shrLoadPPM4ub`'s alpha padding. Neither the PPM data nor any other input or
result file is vendored in this repository.

The upstream image remains available from the pinned HeCBench checkout:
`src/medianfilter-sycl/data/SierrasRGB.ppm` in ORNL/HeCBench at
`0a17ee47ca6072192101036d4bf292e6612f22e6`.

## Provenance

- Reference: [ORNL/HeCBench](https://github.com/ORNL/HeCBench) at commit
  `0a17ee47ca6072192101036d4bf292e6612f22e6`.
  - CUDA: `src/medianfilter-cuda/main.cu`, `src/medianfilter-cuda/MedianFilter.cu`,
    `src/medianfilter-cuda/MedianFilterHost.cpp`.
  - OpenMP: `src/medianfilter-omp/main.cpp`.
- Source port: `psychocoderHPC-bot/alpaka3` PR #44, head
  `b6fa1cbae28351d4ec234f8acbf30ece886a70ee`, `benchmark/medianfilter`.
- Original: the NVIDIA CUDA SDK median filter sample. Its sources are kept
  outside this repository and are not vendored; only this clean-room alpaka
  reimplementation is committed under `BSD-3-Clause`.

## Build and run

Build from the repository root with the CPU serial preset:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target medianfilter-alpaka -j
```

Run a short explicit configuration:

```bash
./build/cpu-serial/bin/alpaka/medianfilter --width 257 --height 255 --repeat 4 --samples 2
```

Run the upstream positional form (the image path is ignored):

```bash
./build/cpu-serial/bin/alpaka/medianfilter ../medianfilter-sycl/data/SierrasRGB.ppm 1000
```

The same target builds with the `cuda` preset. The preset defaults to `sm_80`;
pass `-DCMAKE_CUDA_ARCHITECTURES=<n>` to target another GPU.

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target medianfilter-alpaka -j
./build/cuda/bin/alpaka/medianfilter --width 1920 --height 1080 --repeat 100 --samples 5
```

The benchmark prints a `RESULT` line with the median, minimum, maximum and
relative spread of the samples, the raw samples, the upstream-compatible
`Average kernel execution time: <value> (s)` line and a final
`medianfilter verification PASSED` line.

## CTest

The CMake test registration runs a fast subset:

```bash
ctest --test-dir build/cpu-serial -R medianfilter-alpaka --output-on-failure
```
