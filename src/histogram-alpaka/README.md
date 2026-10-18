<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# histogram (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of
the HeCBench `histogram` benchmark. One C++20 translation unit generates an
image of random 8-bit RGBA pixels, histograms it into 256 bins per active
channel with block-private shared-memory counters, and verifies the result
against an independent host reference. It compiles unchanged for the host CPU
and for CUDA GPUs. There is no backend branch and no native
CUDA/HIP/OpenMP kernel.

## Computation

The pinned reference generates an image with MT19937 seeded by
`init_by_array({0x123, 0x234, 0x345, 0x456}, 4)` and the `RandomBits()`
reduction. The port reproduces that generator and the reference bucket scheme,
then runs three genres with 256 bins:

- `uchar1`: one channel, the integer average of the first three input channels;
- `uchar4`: three channels taken directly from the 8-bit pixels;
- `float4`: three channels scaled to `[0, 1)`, then multiplied by 256 and
  truncated.

The kernel is the shared-memory privatised variant of the reference
(`histogram_smem_atomics`). Each block owns a padded shared counter array
(`+ CHANNEL`), every thread atomically increments the counters for its pixels,
and the block writes a per-block partial histogram. A second kernel accumulates
the partials into the final histogram. A grid-stride loop partitions the image,
so every pixel belongs to exactly one block/thread pair for any positive block
count.

## Command line

```text
./histogram [--pixels <n>] [--repeat <n>] [--samples <n>] [--entropy <n>] [--verify-only]
./histogram [--width <n>] [--height <n>] [--i <n>] [--samples <n>] [--entropy <n>] [--verify-only]
```

Long options accept `--name value` and `--name=value`. Defaults follow the
reference: `--pixels 1920*1080`, `--repeat 100`, `--samples 5`,
`--entropy 0`. The reference spellings `--width`/`--height` set the pixel count
to their product, and `--i` is an alias for `--repeat`. `--verify-only` runs
the correctness checks without the timing loop. The reference histogram has no
positional form, so a bare positional argument is rejected. `-h`/`--help`
prints the usage text. Invalid or missing option values and out-of-range
parameters exit non-zero.

## What the test validates

`helper.hpp` reimplements the reference MT19937 and `RandomBits()` so the host
produces the same pixels, and it computes an independent host histogram from
those pixels. After a fresh device run the result is copied back and compared
bin by bin with an exact integer comparison, plus the total-count invariant
`sum(hist) == ACTIVE_CHANNELS * numPixels`. Any difference prints the first
mismatching `(channel, bin)` and exits non-zero. The `float4` input is also
checked finite before use.

A histogram is an integer reduction of exact bucket assignments, so no
tolerance is applied; the HeCBench reference also compares `unsigned int`
counts with `!=`.

The registered CTest case uses the upstream `subset.json` geometry
`--width=3840 --height=2160`, with the reference defaults for repetitions and
samples. It passes only when the timing line appears and every genre verifies.

## Provenance

- HeCBench reference pinned at
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/tree/0a17ee47ca6072192101036d4bf292e6612f22e6):
  `src/histogram-cuda/histogram_compare.cu`, `histogram_smem_atomics.h`,
  `mersenne.h` and `test_util.h`, plus the OpenMP twin
  `src/histogram-omp/histogram_compare_base.cpp`. The upstream sources are kept
  outside this repository and are not vendored.
- Original alpaka port: alpaka3 PR #34 head `930c618c7e96`,
  `benchmark/histogram`.
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as
  the `extern/alpaka` submodule.

The port and helper files are new work under BSD-3-Clause. The MT19937
implementation reproduced in `helper.hpp` follows the BSD-3-Clause notices in
the reference `mersenne.h` (Makoto Matsumoto and Takuji Nishimura) and
`test_util.h` (Duane Merrill; NVIDIA Corporation). HeCBench itself is
BSD-3-Clause (Argonne National Laboratory, Oak Ridge National Laboratory).

## Build, run and test

CPU serial, Release:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target histogram-alpaka -j
./build/cpu-serial/bin/alpaka/histogram --pixels 100000 --repeat 4 --samples 2
ctest --test-dir build/cpu-serial -R histogram-alpaka --output-on-failure
```

CPU with the OpenMP block executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target histogram-alpaka -j
./build/cpu/bin/alpaka/histogram --width 1920 --height 1080 --i 20
```

CUDA (architecture set by the preset, default `sm_80`):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target histogram-alpaka -j
./build/cuda/bin/alpaka/histogram --width 3840 --height 2160
```

The executable runs every enabled backend/executor and reports each genre
separately. Timing brackets `repeat` back-to-back histogram computations
between synchronisations and divides by `repeat`, matching the reference. One
warm-up computation precedes the measured samples, and the median of the
samples is reported. Fast math is off by default because the native HeCBench
references use plain `-O3`.

The upstream harness can run the port through the vendored tooling:

```bash
python3 tools/hecbench -v run histogram --model alpaka
```

This repository does not ship measured results, plots or comparison tables.
