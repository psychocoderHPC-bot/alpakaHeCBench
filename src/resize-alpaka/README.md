<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# resize (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of
the HeCBench `resize` benchmark. One C++20 translation unit resizes a stack of
single-channel 8-, 16- and 32-bit images with a nearest-neighbour and a bilinear
kernel. The same source compiles for the host CPU and for GPU backends; there is
no backend branch and no native CUDA/HIP/OpenMP kernel.

## Computation

The reference resizes `num_channels` single-channel images, each
`in_height x in_width`, to `out_height x out_width`. For each output pixel the
nearest kernel maps the source coordinate to `lroundf((idx + 0.5f) * o2i)` and
clamps it to the source extent. The bilinear kernel maps it to
`fmaxf((idx + 0.5f) * o2i - 0.5f, 0)` and combines the four neighbours:

```text
v00 + T(fy)*T(v10-v00) + T(fx)*T(v01-v00) + T(fy)*T(fx)*T(v11-v01-v10+v00)
```

The port preserves four reference behaviours that a textbook resize would get
wrong:

1. **Integer-division scaling ratios.** `resize_image` computes
   `const float fx = in_width / out_width;` with integer division, so
   `1920/256 == 7` rather than `7.5`.
2. **Swapped axis ratios.** `resize_image` passes `(fx, fy)` to the kernel
   parameters `(o2i_fy, o2i_fx)`; `o2i_fy` receives `in_width / out_width` and
   `o2i_fx` receives `in_height / out_height`.
3. **Integer weight cast.** The bilinear weights `T(in_y - in_y0)` and
   `T(in_x - in_x0)` are cast to the pixel type `T`. For the integer pixel types
   the fractional parts truncate to `0`, so the bilinear kernel degenerates to
   `v00`.
4. **Group-of-eight channel truncation.** Both kernels process
   `CHANNELS_PER_ITER = 8` channels per work item and compute
   `num_channel_iters_per_xy = num_channels / 8` with integer division. Channels
   `[(num_channels/8)*8, num_channels)` are never written and keep their
   zero-initialised value.

The input is generated deterministically, so the repository carries no image
data.

## Command line

The HeCBench positional form is accepted:

```text
resize <in_width> <in_height> <out_width> <out_height> <channels> <repeat>
```

as well as named options:

```text
resize --in-width W --in-height H --out-width W2 --out-height H2 --channels C \
       --repeat N --samples S --types T --nearest --bilinear --verify-only
```

Defaults are `1920x1080 -> 256x256`, `channels=512`, `repeat=100` and
`samples=5`, matching the reference `run` target `1920 1080 256 256 512 100`.
`--types` selects pixel type sizes in bytes (`1`, `2`, `4`), is repeatable and
defaults to all three. `--nearest` and `--bilinear` are both enabled by default.
`--verify-only` skips the timing loop. `misc.hpp` has no Catch2 dependency; it
uses a small `std::string` parser that keeps the same options, defaults,
positional form and validation errors.

## Test semantics

`helper.hpp` contains an independent host implementation of the same pipeline in
the same pixel type `T`. It fills the input with `(i+1) % 13`, zero-initialises
the output like the reference `cudaMemset`, reproduces the integer-division
ratios and the swapped `(o2i_fy, o2i_fx)` order, and evaluates the same
expression including the integer weight cast and the group-of-eight channel
truncation. After a fresh kernel run the port copies the output back and:

- checks the output for non-finite values;
- compares every element with the host reference exactly (tolerance 0).

A mismatch exits non-zero. The arithmetic is integer for all three pixel types
and both sides evaluate the identical expression in `T`, so widening the
tolerance would only mask coordinate, cast or truncation bugs.

The CTest case runs a small odd-sized resize for all pixel types and both
kernels:

```text
--in-width 257 --in-height 129 --out-width 65 --out-height 33 --channels 8 --repeat 4 --samples 2
```

Upstream HeCBench has no `subset.json` or `benchmarks.yaml` test metadata for
`resize`; the local test invocation and the new `subset.json` entry follow the
source port's test arguments and output format.

## Build and test

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target resize-alpaka -j
ctest --test-dir build/cpu-serial -R resize-alpaka --output-on-failure
```

Run the benchmark directly:

```bash
./build/cpu-serial/bin/alpaka/resize \
  --in-width 1920 --in-height 1080 --out-width 256 --out-height 256 \
  --channels 512 --repeat 100 --samples 5
```

The `cpu` and `cuda` presets build the same target. The vendored harness can run
it with `python3 tools/hecbench -v run resize --model alpaka`.

## Provenance

- HeCBench reference at
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/commit/0a17ee47ca6072192101036d4bf292e6612f22e6):
  `src/resize-cuda/main.cu` and `src/resize-omp/main.cpp`; the SYCL variant
  `src/resize-sycl/main.cpp` was consulted for the cast semantics.
- Original alpaka port: alpaka3 PR #52, head
  [`10ab4d09ef92`](https://github.com/psychocoderHPC-bot/alpaka3/commit/10ab4d09ef92eb9960ae96c02caba52472e459a1),
  `benchmark/resize`.
- alpaka pin: [`b7d339d07056a9a2a6c4051cc3927157bc5f0d51`](https://github.com/alpaka-group/alpaka/commit/b7d339d07056a9a2a6c4051cc3927157bc5f0d51).

Reference sources are kept outside this repository and are not vendored. The
implementation, helper and parser are BSD-3-Clause. This branch carries no
result files, measurement data, plots or comparison tables.
