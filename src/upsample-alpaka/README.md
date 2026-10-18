<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# upsample (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of
the HeCBench `upsample` benchmark. One C++20 translation unit implements the 2x
nearest-neighbour upsampling layer of a small U-Net and is compiled unchanged
for the host CPU and for a CUDA GPU. There is no backend branch and no native
CUDA/HIP/OpenMP kernel.

## What it computes

The reference operates on an `N, C, H, W` tensor (batch, channel, height,
width) with row-major contiguous storage. It has two passes:

- **Forward:** every input element `(b, c, i, j)` is replicated into the four
  output positions `(2i, 2j)`, `(2i, 2j+1)`, `(2i+1, 2j)` and `(2i+1, 2j+1)` of
  the doubled `2H x 2W` image.
- **Backward:** every gradient element `(b, c, i, j)` is the sum of the four
  `dout` values of the same 2x2 tile.

The reference defines two kernel formulations per pass: a flat
one-thread-per-element kernel and a 2D tile kernel. Both compute the same
operations. This port keeps the flat formulation, which the reference
benchmarks first, and uses the same index decomposition. The backward sum uses
the reference order `(((v00 + v01) + v10) + v11)`, so a correct port reproduces
the reference single-precision arithmetic exactly.

The port keeps one portable kernel per pass. The work item count and logical
256-thread mapping are backend-agnostic; the executor chooses the physical
mapping.

## Parameters

- Positional HeCBench form:
  `upsample <batch> <channels> <height> <width> <repeat>`.
- Named form: `upsample --batch N --channels C --height H --width W --repeat R
  --samples S [--seed S] [--verify-only]`.
- `--batch` / `--channels` / `--height` / `--width` default to `32` / `64` /
  `64` / `64`, matching the reference `run` target (`32 64 64 64 100`).
- `--repeat` defaults to `100`: passes per measured sample.
- `--samples` defaults to `5`: independent measured samples per pass.
- `--seed` defaults to `0`: seed of the internal deterministic input generator.
  The reference calls `srand(0)` and fills its buffers with
  `rand()/RAND_MAX*2-1` in `[-1, 1)`. That generator is not thread-safe and
  depends on the OpenMP thread count, so this port uses a deterministic
  xorshift64* generator with the same range.
- `--verify-only` runs the correctness check without the timing loop.
- `-h` / `--help` prints the usage.

The positional form and the matching named options cannot be combined for the
same field. All sizes must be greater than zero. Single precision matches the
reference; no other precision or mode options are exposed.

## Test semantics

Two trusted host references are recomputed from the same deterministic input:

- `forwardReference` / `backwardReference` (`helper.hpp`) are single-precision
  references in the reference's exact accumulation order. The accelerator
  result is compared against them. Because both operations are a fixed-order
  sum of at most four values, a correct port matches them bit-for-bit.
- `forwardOracleDouble` / `backwardOracleDouble` are independent
  double-precision oracles that do not share the single-precision accumulation
  order. `verifyReferenceAgreement` pins the single-precision references to
  within fp32 rounding and guards against a shared bug in two single-precision
  reimplementations.

`verifyForward` compares the output tile-by-tile with the per-tile relative-L2
metric `sqrt(sum_tile(delta^2) / sum_tile(ref^2))`. `verifyBackward` uses
`|got - ref| / |ref|` per element. Both reject any NaN or Inf with a nonzero
exit code. The per-tile metric cannot hide a single wrong tile inside a large
tensor the way one global norm can. The acceptance threshold is `1e-8`, below
one fp32 unit roundoff (`~6e-8`), so a wrong tile mapping, border rule or offset
cannot pass. A nonzero exit code signals failure.

The CTest registration exercises odd spatial sizes (`H=31`, `W=33`) in addition
to the default `64x64`.

## Input data

The reference builds its input with `srand(0)` / `make_random_float`. This port
is self-contained: `makeRandomFloat` (`helper.hpp`) produces a deterministic
input in `[-1, 1)` with a xorshift64* generator. No input or result file is
vendored in this repository.

## Provenance

- Reference: [ORNL/HeCBench](https://github.com/ORNL/HeCBench) at commit
  `0a17ee47ca6072192101036d4bf292e6612f22e6`.
  - CUDA: `src/upsample-cuda/main.cu`, `src/upsample-cuda/reference.h`,
    `src/upsample-cuda/common.h`.
  - OpenMP: `src/upsample-omp/main.cpp`.
- Source port: `psychocoderHPC-bot/alpaka3` PR #50, head
  `ecdccea849d2e2d89e1296858467905757aef5ba`, `benchmark/upsample`.
- Original: [`clu0/unet.cu`](https://github.com/clu0/unet.cu)
  `train_unet.cu`, `upsample_forward_kernel` and `upsample_backward_kernel`.
  The repository carries no explicit license file; it is referenced here only
  as provenance and is not vendored.
- Upstream HeCBench license: BSD-3-Clause (Copyright (c) 2020 Argonne National
  Laboratory (Zheming Jin); Copyright (c) 2020- Oak Ridge National Laboratory
  (Zheming Jin)). The implementation files in this directory are
  `BSD-3-Clause`; `misc.hpp` contains no HeCBench code.

## Build and run

Build from the repository root with the CPU serial preset:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target upsample-alpaka -j
```

Run a short explicit configuration:

```bash
./build/cpu-serial/bin/alpaka/upsample \
  --batch 4 --channels 8 --height 31 --width 33 --repeat 4 --samples 2
```

Run the upstream positional form at the reference size:

```bash
./build/cpu-serial/bin/alpaka/upsample 32 64 64 64 100
```

The same target builds with the `cuda` preset. The preset defaults to `sm_80`;
pass `-DCMAKE_CUDA_ARCHITECTURES=<n>` to target another GPU.

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target upsample-alpaka -j
./build/cuda/bin/alpaka/upsample --batch 32 --channels 64 --height 64 --width 64 \
  --repeat 100 --samples 5
```

The benchmark prints a `RESULT` line per pass with the median, minimum, maximum
and relative spread of the samples, the raw samples, and a final
`upsample ... verification PASSED` line.

## CTest

The CMake test registration runs a fast subset:

```bash
ctest --test-dir build/cpu-serial -R upsample-alpaka --output-on-failure
```
