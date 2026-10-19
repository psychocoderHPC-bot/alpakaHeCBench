<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# heat2d (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of
the HeCBench `heat2d` benchmark. One C++20 translation unit advances the 2D
explicit heat equation and is compiled unchanged for the host CPU and for GPU
backends. There is no backend branch and no native CUDA/HIP/OpenMP kernel.

## Computation

The grid is `Lx x Ly` in row-major order with periodic boundaries in both
directions. One five-point stencil step is

```text
out[y*Lx + x] = norm * in[y*Lx + x]
                + delta * ( in[y*Lx + (x+1)%Lx]
                          + in[y*Lx + (Lx+x-1)%Lx]
                          + in[((y+1)%Ly)*Lx + x]
                          + in[((Ly+y-1)%Ly)*Lx + x] )
```

with `sigma = 0.01`, `delta = sigma/(1+4*sigma)` and `norm = 1/(1+4*sigma)`.
The algorithm launches one kernel per iteration, `niter` times, and swaps the
two field buffers after each step. A flat 1D buffer keeps the indexing
independent of any backend pitch convention.

The coefficients are stored as `float` but the division is evaluated in double
and rounded once, matching the reference expression `sigma / (1.0 + 4.0*sigma)`.

The initial field reproduces the pinned reference: `srand(123)`, then a `1` is
stamped into pseudo-random rows and columns every 16 entries. The reference
leaves the remaining cells uninitialised; this port zero-initialises them so the
input is deterministic. The stencil and the periodic wrap are unchanged.

## Command line

The upstream positional form is

```text
./heat2d [<Lx> <Ly> <niter> [<repeat>]] [--all-backends]
```

and the named form is

```text
./heat2d [--lx <n>] [--ly <n>] [--iters <n>] [--repeat <n>] [--all-backends]
```

Defaults are `Lx = 4096`, `Ly = 4096`, `niter = 1000` and `repeat = 1`.
`niter` must be at least one. Named options also accept `--name=value`.
Invalid, missing or out-of-range values exit non-zero; `Lx*Ly` must fit in
`int`.

By default the executable runs exactly one backend/executor and prints exactly
one `usec/iter` line. It selects the first available non-CPU accelerator (for
example CUDA, HIP or SYCL), otherwise CPU OpenMP blocks, otherwise CPU serial.
`NumaCpu` is skipped because it duplicates the CPU device. Pass
`--all-backends` to run every enabled backend/executor instead; that mode prints
one `usec/iter` line per backend, and the vendored harness sums those lines, so
do not use it for a single reported timing.

For each measured run both device buffers are reset to the initial field, the
`niter` launches are submitted back-to-back on a non-blocking queue, and the
run is synchronised once. One untimed warm-up run precedes the measured runs.
The port prints the upstream-compatible `usec/iter` token in addition to its own
result line.

## Test semantics

After the final swap the newest device field is copied to the host and compared
with an independent double-precision host reference built from the same initial
field and the same iteration count. The reference shares no indexing helper,
tiling or launch configuration with the kernel. Every device value must be
finite, and the maximum absolute difference must stay within
`1e-5 + 1e-7*niter`. A failure prints the offending index and tolerance and
exits non-zero.

The registered CTest case runs the upstream subset arguments
`1024 1024 100`, requires the `usec/iter` line and passes only if the
verification succeeds.

## Provenance

- HeCBench `heat2d` reference: commit
  `0a17ee47ca6072192101036d4bf292e6612f22e6`
  (`src/heat2d-cuda`, `src/heat2d-hip`, `src/heat2d-omp`, `src/heat2d-sycl`).
- Original alpaka port: alpaka3 PR #39 head `99f784cd3143`,
  `benchmark/heat2d`.
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as
  the `extern/alpaka` submodule.

The port and the helper files are new work under BSD-3-Clause. The benchmark
source and validation code are unchanged from the original alpaka port; only
the Catch2 command-line parsing was replaced with a small `std::string` parser.

## Build and run

CPU serial, Release:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target heat2d-alpaka -j
./build/cpu-serial/bin/alpaka/heat2d 1024 1024 100
```

CPU with the OpenMP executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target heat2d-alpaka -j
./build/cpu/bin/alpaka/heat2d --lx 1024 --ly 1024 --iters 100
```

CUDA (`sm_80` by default):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target heat2d-alpaka -j
./build/cuda/bin/alpaka/heat2d 1024 1024 100
```

The executable runs one backend/executor by default. Add `--all-backends` to
run every enabled combination; each one then reports its own timing.

## Test

```bash
ctest --test-dir build/cpu-serial -R heat2d-alpaka --output-on-failure
```

The upstream harness can run the port through the vendored tooling:

```bash
python3 tools/hecbench -v run heat2d --model alpaka
```

`compile` and `run` phases are supported; this repository does not ship measured
results, plots or comparison tables.
