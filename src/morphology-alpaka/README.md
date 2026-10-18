<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# morphology (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`morphology` benchmark. One C++20 translation unit applies a 2D separable morphological dilation
and erosion with a rectangular structuring element to an 8-bit image and verifies the result
against an independent host reference. The same source runs on the host CPU and on CUDA GPUs;
there is no backend branch and no native CUDA/HIP/OpenMP kernel.

## Computation

The pinned HeCBench reference (`src/morphology-cuda` and `src/morphology-omp` at
`0a17ee47ca6072192101036d4bf292e6612f22e6`) implements dilation and erosion with the van
Herk/Gil-Werman algorithm. For an output cell:

- dilation is the maximum over the rectangular structuring-element window, with border value
  `255` (`WHITE`),
- erosion is the minimum over the window, with border value `0` (`BLACK`).

The reference builds a black image with a single white pixel at `(height/2 - 1, width/2 - 1)`,
then applies one dilation and one erosion in place per repeat. The intermediate image is cleared
before each operation. The port keeps the same pass order (one horizontal pass, then one vertical
pass), the same odd/even radii, and the same border rules: horizontal output is written only for
columns `[hsize/2, width - hsize/2)` and untouched intermediate columns stay zero; vertical rows
outside `[vsize/2, height - vsize/2)` are set to the operation's border value.

The portable kernel replaces the reference block scan with a thread-local monotonic deque over a
segment of a row or column. Each output element costs amortised O(1) and the cost depends on the
structuring-element size only through the segment overlap. On the host a work item maps to one
OpenMP or serial iteration; on CUDA it maps to one thread.

The datapath is integer `min`/`max` over `uint8_t` values. There is no floating-point accumulation
and no tolerance.

## Command line

Two forms are accepted. Named options:

```text
morphology [--hsize N] [--vsize N] [--width N] [--height N] [--repeat N]
           [--samples N] [--seed N] [--verify-only]
```

`--option=value` is accepted as well. The native HeCBench positional form is also accepted:

```text
morphology <kernel width> <kernel height> <image width> <image height> <repeat>
```

Named options and positional arguments cannot be mixed. Defaults are `999x777` for the image,
`71x71` for the structuring element, `100` dilate+erode pairs per measured sample, `5` samples,
and `--seed 0` for the reference's single-white-pixel image. A nonzero seed generates a
deterministic pseudo-random image. `--verify-only` skips the timing loop.

`hsize` and `vsize` must be positive and at most `256`; width, height, repeat and samples must be
positive. Invalid input prints an error and usage text and exits non-zero. The image is generated
internally, so the port takes no image path and vendors no image data.

## Output and test semantics

A normal run prints one informational header line, one `RESULT ... median_us=...` line, one raw
sample line, the reference's reproduction line `image byte-sum=... (reference report line:
PASS|FAIL)`, and a final `morphology WxH hsize=... vsize=... verification PASSED` line.

The authoritative test compares the complete accelerator output byte by byte against an
independent O(window-size) host reference after the same number of dilate+erode pairs. The first
mismatch reports the index and the worst absolute difference and exits non-zero. For the
reference image (`seed 0`) the output is also checked to contain only the two reference levels.
The `sum(image) == 255` line is informational because the pinned reference prints it without
changing its exit code.

CTest registers a fast correctness case for the source port's test configuration and a boundary
case for the maximum structuring element (`hsize = vsize = 256`). Both use the verification line
as the success expression.

## Provenance

- HeCBench reference: commit
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/commit/0a17ee47ca6072192101036d4bf292e6612f22e6),
  `src/morphology-cuda` and `src/morphology-omp`.
- Source port: alpaka3 PR #42, head
  [`f14827a05a343459746c543e52f05ac1f7b6b11c`](https://github.com/psychocoderHPC-bot/alpaka3/commit/f14827a05a343459746c543e52f05ac1f7b6b11c),
  `benchmark/morphology`.
- alpaka pin: commit `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the
  `extern/alpaka` submodule.

The reference sources are not vendored. Obtain them by cloning ORNL/HeCBench and checking out the
pinned commit above. The benchmark needs no input file: the image is generated in code.

The implementation and host reference are BSD-3-Clause; this README is CC0-1.0.

## Build and run

CPU serial (Release, source-tree alpaka):

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target morphology-alpaka -j
./build/cpu-serial/bin/alpaka/morphology --width 999 --height 777 --hsize 71 --vsize 71 --repeat 20
```

CPU with the OpenMP block executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target morphology-alpaka -j
OMP_NUM_THREADS=48 ./build/cpu/bin/alpaka/morphology --width 999 --height 777 --hsize 71 --vsize 71
```

CUDA:

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target morphology-alpaka -j
./build/cuda/bin/alpaka/morphology --width 999 --height 777 --hsize 71 --vsize 71 --repeat 100
```

## Test

```bash
ctest --test-dir build/cpu-serial -R morphology-alpaka --output-on-failure
```
