<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# laplace3d (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of
the HeCBench `laplace3d` benchmark. One C++20 translation unit runs the
seven-point 3D Laplacian Jacobi kernel on the host CPU and on GPU backends.
There is no backend branch and no native CUDA/HIP/OpenMP kernel.

## Computation

The grid is `Nx x Ny x Nz` in row-major order with x innermost
(`ind = i + j*Nx + k*Nx*Ny`). Boundary cells are Dirichlet cells and keep their
value. Every interior cell is replaced by the average of its six axis
neighbours:

```text
out[ind] = ( in[ind-1]     + in[ind+1]
           + in[ind-Nx]    + in[ind+Nx]
           + in[ind-Nx*Ny] + in[ind+Nx*Ny] ) * (1/6)
```

The initial field has `1.0f` on every boundary cell and `0.0f` in the interior.
The kernel runs once per iteration, `niter` times, and the two field buffers
swap after each step like the reference `d_u1`/`d_u2` ping-pong. A flat 1D
buffer keeps the indexing independent of any backend pitch convention.

## Command line

The upstream positional form is

```text
./laplace3d [<Nx> <Ny> <Nz> <niter> [<repeat>]]
```

and the named form is

```text
./laplace3d [--nx <n>] [--ny <n>] [--nz <n>] [--iters <n>] [--repeat <n>]
```

Defaults are `Nx = Ny = Nz = 128`, `niter = 100` and `repeat = 5`. Named
options also accept `--name=value`. Invalid, missing or out-of-range values
exit non-zero; `Nx*Ny*Nz` must fit in `int`.

For each measured run both device buffers are reset to the initial field, the
`niter` launches are submitted back-to-back on a non-blocking queue, and the
run is synchronised once. One untimed warm-up run precedes the measured runs.
The port prints the upstream-compatible `Average kernel execution time: X (s)`
token in addition to its own result line.

## Test semantics

After the final swap the newest device field is copied to the host and compared
with an independent double-precision host reference built from the same initial
field and the same iteration count. The reference shares no indexing helper,
boundary predicate, tiling or launch configuration with the kernel. Every
device value must be finite, and the maximum absolute difference must stay
within `1e-4 + 1e-6*niter`. A failure prints the offending index and tolerance
and exits non-zero.

The registered CTest case runs the upstream subset arguments
`128 128 128 100 1`, requires the `Average kernel execution time: ... (s)`
line and passes only if the verification succeeds.

## Provenance

- HeCBench `laplace3d` reference: commit
  `0a17ee47ca6072192101036d4bf292e6612f22e6`
  (`src/laplace3d-cuda`, `src/laplace3d-omp`). The reference sources stay
  outside this repository.
- Original alpaka port: alpaka3 PR #45 head `7b697d129649`,
  `benchmark/laplace3d`.
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as
  the `extern/alpaka` submodule.

The port and the helper files are new work under BSD-3-Clause. The kernel and
validation code are unchanged from the original alpaka port; only the Catch2
command-line parsing was replaced with a small `std::string` parser.

## Build and run

CPU serial, Release:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target laplace3d-alpaka -j
./build/cpu-serial/bin/alpaka/laplace3d 128 128 128 100 1
```

CPU with the OpenMP executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target laplace3d-alpaka -j
./build/cpu/bin/alpaka/laplace3d --nx 128 --ny 128 --nz 128 --iters 100
```

CUDA (`sm_80` by default):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target laplace3d-alpaka -j
./build/cuda/bin/alpaka/laplace3d 128 128 128 100 1
```

The executable runs every enabled backend/executor and reports each one
separately.

## Test

```bash
ctest --test-dir build/cpu-serial -R laplace3d-alpaka --output-on-failure
```

The upstream harness can run the port through the vendored tooling:

```bash
python3 tools/hecbench -v run laplace3d --model alpaka
```

This repository does not ship measured results, plots or comparison tables.
