<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# dct8x8 (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`dct8x8` benchmark. One C++20 translation unit applies a separable 8x8 DCT, or its inverse, to
every full 8x8 block of a `float` image. The same source compiles for the host CPU and for a CUDA
GPU; there is no backend branch and no native CUDA, HIP or OpenMP kernel.

## Computation

The pinned HeCBench reference (`src/dct8x8-cuda` and its OpenMP twin `src/dct8x8-omp`) transforms
a `width x height` image block by block:

- the forward transform is an orthonormal DCT-II (`sqrt(1/8)` for the DC term, `sqrt(2/8)`
  otherwise), applied separably over rows and then columns;
- the inverse transform is its transpose (DCT-III);
- the reference uses the hardcoded AAN fast transform from the NVIDIA CUDA SDK sample with the
  constants in `DCT8x8.h`; this port keeps those exact constants and the same row-then-column
  pass order;
- only full 8x8 blocks are transformed. Cells outside a full block keep the input value, which
  defines the reference's otherwise uninitialized remainder.

`main` generates the input with `srand(2009); x = (float)rand() / (float)RAND_MAX;`, copies it to
the accelerator, optionally times repeated transforms, then runs one transform and validates the
result against an independent double-precision host reference.

## Test semantics

`helper.hpp` contains a deliberately different, straightforward double-precision matrix
implementation of the same separable DCT-II/DCT-III pair. It is the oracle, not the timed code.
Validation applies the same full-block rule, computes the reference's relative L2 metric
`sqrt(sum((got - ref)^2) / sum(ref^2))`, rejects any NaN or Inf, and requires the ratio to stay
below the reference's own `1e-6` tolerance. The process exits non-zero on failure.

The CTest registration uses the upstream run workload and arguments (`8192 8192 100`) and matches
the upstream timing line. A separate `--verify-only` run performs the same validation
without the timing loop.

## Provenance

- HeCBench reference: commit
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/commit/0a17ee47ca6072192101036d4bf292e6612f22e6),
  `src/dct8x8-cuda/{main.cu,kernels.cu,DCT8x8_gold.cu,DCT8x8.h}` and
  `src/dct8x8-omp/{main.cpp,kernels.cpp,DCT8x8_gold.cpp,DCT8x8.h}`.
- Original alpaka port: alpaka3 PR #47 head `5cdda3319211` (`benchmark/dct8x8/src`).
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the
  `extern/alpaka` submodule.
- Reference sources are not vendored. The original algorithm is the NVIDIA CUDA SDK `dct8x8`
  sample (AAN 8-point fast DCT).

## Build

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target dct8x8-alpaka -j
```

For CUDA use the `cuda` preset and select the target architecture when it differs from `sm_80`:

```bash
cmake --preset cuda -DCMAKE_CUDA_ARCHITECTURES=80 -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target dct8x8-alpaka -j
```

The binary is written to `build/<preset>/bin/alpaka/dct8x8`. `make -C src/dct8x8-alpaka` wraps
the same Release build and places the binary at `src/dct8x8-alpaka/main` for the HeCBench tools.

## Run

The named form accepts `--width`, `--height`, `--repeat`, `--samples`, `--seed`, `--inverse` and
`--verify-only`. Defaults are `8192 8192 100 5 2009`, matching the upstream run target and seed.
The reference's positional form is also accepted:

```bash
./build/cpu-serial/bin/alpaka/dct8x8 8192 8192 100
./build/cpu-serial/bin/alpaka/dct8x8 --width 999 --height 777 --repeat 20 --samples 2 --inverse
./build/cpu-serial/bin/alpaka/dct8x8 --width 999 --height 777 --verify-only
```

## Test

```bash
ctest --test-dir build/cpu-serial -R dct8x8-alpaka --output-on-failure
```

This runs the benchmark with the upstream workload arguments and checks for the upstream
`Average DCT8x8 kernel execution time ... (s)` line. Validation output is printed on stdout and a
non-zero exit code marks a failure.

## License

The benchmark sources are `BSD-3-Clause`. They are re-derived and contain no HeCBench code.
