<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# stencil1d (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`stencil1d` benchmark. One C++20 translation unit applies a radius-7 1D sliding-sum stencil to a
padded input array. The same source compiles unchanged for the host CPU and for a CUDA GPU; there is
no backend branch and no native CUDA/HIP/OpenMP kernel.

## What it computes

For `length` output elements and a padded input `a` with `length + RADIUS` elements:

```text
out[i] = sum_{k=-RADIUS}^{RADIUS} a[i+k]     with RADIUS = 7
```

Positions before the domain and at or beyond the padded buffer `[0, length + RADIUS)` are zero.
The tile size is `BLOCK_SIZE = 256`. Each thread block stages a `BLOCK_SIZE + 2*RADIUS = 270`
element window in shared memory: `[0, RADIUS)` is the left halo, `[RADIUS, RADIUS + tileSize)` the
centre and `[RADIUS + tileSize, tileSize + 2*RADIUS)` the right halo. Threads with
`localId < RADIUS` load both halos, all threads load the centre, the block synchronises and then
sums 15 neighbours per output element. Outputs at or beyond `length` are not written, so any positive
`length` is accepted, including values that are not a multiple of the tile size.

The input is a deterministic xorshift pattern in `[-4096, 4095]` (`makeInput`), not the monotone ramp
`a[i] = i` used by the upstream harness. A ramp would hide an aliasing error: for a symmetric sliding
sum a duplicated or shifted halo reproduces the same sum of consecutive integers. The pseudo-random
pattern is not shift invariant.

## Provenance

- HeCBench reference: [ORNL/HeCBench](https://github.com/ORNL/HeCBench) at commit
  `0a17ee47ca6072192101036d4bf292e6612f22e6`.
  - CUDA reference: `src/stencil1d-cuda/stencil_1d.cu`
  - OpenMP reference: `src/stencil1d-omp/stencil_1d.cpp`
- Original alpaka port: PR #35 head `081b2f325e2a` (`benchmark/stencil1d/src/main_alpaka.cpp`,
  `helper.hpp`, `misc.hpp`). The translation unit is named `main.cpp` here to match this repository's
  layout; the kernel and reference are unchanged. The Catch2 dependency of the original command-line
  helper was replaced with a small standard-library parser.
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the `extern/alpaka`
  submodule.
- Reference sources are kept outside this repository and are not vendored. The upstream HeCBench
  license applies to the benchmark definition.

## Command line

| form | example |
| --- | --- |
| upstream positional | `stencil1d <length> <repeat>` |
| named | `stencil1d --length <N> --repeat <R>` |
| named assignment | `stencil1d --length=<N> --repeat=<R>` |
| diagnostic | `stencil1d --length <N> --repeat <R> --all-backends` |

`length` defaults to `1 << 20` and `repeat` to `5`. Both arguments must be positive decimal integers.
Invalid, missing or unknown arguments print an error to stderr and exit non-zero. Named options take
precedence over the positional form.

By default the program runs exactly one backend and prints exactly one timing line. It selects the
first available non-host accelerator; if none is available it uses the host CPU with `CpuOmpBlocks`,
falling back to `CpuSerial`. The `NumaCpu` device spec is skipped because it duplicates the host CPU
device on hwloc builds. `--all-backends` is an opt-in diagnostic that runs every available backend
and prints one line per backend; do not use it with the benchmark harness, which sums all matching
lines.

The measured metric is the kernel-compute time: `repeat` back-to-back kernel launches between two
synchronisations, divided by `repeat`. One warm-up launch runs before the measured region. The
program prints one line, `stencil1d kernel runtime (batch of <repeat>): <value> us/kernel`.

## Test semantics

`src/helper.hpp` contains a double-precision host reference. It sums the window `[i - RADIUS,
i + RADIUS]` by reading the padded input element by element. It does not reuse the kernel's
shared-memory window or its indexing, so a wrong halo load or boundary handling cannot pass.

Two guards back the comparison:

- The default input is non-trivial, so an aliasing halo error changes the result.
- `oracleSanityCheck()` builds the correct output and a deliberately broken output (right-halo
  threads writing into the centre region) from the same input, and requires the comparison to accept
  the first and reject the second.

The arithmetic is integer addition, so the reference is exact and `verify()` compares bit-exactly
(tolerance 0). NaN and Inf are rejected explicitly. A non-zero exit code signals failure; a pass
prints `Stencil1d verification PASSED`.

The CTest case runs `--length 1021 --repeat 2` and requires the runtime line above. `1021` is not a
multiple of the tile size, so the last partial block is exercised. This pair is a smoke test only.

The campaign workload in `src/scripts/benchmarks/subset.json` and the mirrored `benchmarks.yaml`
test args is `--length 134217728 --repeat 1000`, the ORNL canonical run. `134217728 = 2^27` is a
multiple of `BLOCK_SIZE = 256`, so both the native ORNL variants and this port handle the full
domain without a partial trailing block.

## Build and run

CPU serial:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target stencil1d-alpaka -j
./build/cpu-serial/bin/alpaka/stencil1d --length 1048576 --repeat 5
```

CPU with the OpenMP block executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target stencil1d-alpaka -j
OMP_NUM_THREADS=48 ./build/cpu/bin/alpaka/stencil1d --length 1048576 --repeat 5
```

CUDA (default `sm_80`):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target stencil1d-alpaka -j
./build/cuda/bin/alpaka/stencil1d --length 1048576 --repeat 5
```

## Test

```bash
ctest --test-dir build/cpu-serial -R stencil1d-alpaka --output-on-failure
```

The upstream tooling discovers `src/stencil1d-alpaka` as benchmark `stencil1d`, model `alpaka`, and
finds the binary under `build/<preset>/bin/alpaka/stencil1d`. Fast math is off by default; pass
`-DalpakaHeCBench_FAST_MATH=ON` (or `-Dalpaka_FAST_MATH=ON`) to match a fast-math reference.
