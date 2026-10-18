<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# jacobi (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`jacobi` benchmark. One C++20 translation unit solves the 2D Laplace equation on an `N x N` grid
with a five-point Jacobi stencil. The same kernel source runs on the host CPU and on a CUDA GPU.
There is no backend branch and no native CUDA, HIP, OpenMP or SYCL kernel.

## Computation

The grid edges hold a fixed sinusoidal boundary condition and the interior starts at zero. Each
iteration updates the interior from the previous field:

```text
f[i][j] = 0.25 * (f_old[i-1][j] + f_old[i+1][j] + f_old[i][j-1] + f_old[i][j+1])
```

The update applies for `1 <= i,j <= N-2`. The solver computes the normalised L2 change

```text
error = sqrt(sum((f_new - f_old)^2) / (N*N))
```

and stops when `error <= tolerance` or when the iteration cap is reached. The data type is
single-precision `float`, matching the HeCBench reference.

The kernel maps one logical frame to a host OpenMP thread block or to a CUDA thread block. Tiles
are `16 x 16`. The field is a flat row-major `N*N` buffer, so indexing does not depend on a
backend-specific multi-dimensional pitch convention. The block reduction uses warp shuffles plus
one shared-memory step across warps.

## Command line

```text
./main <n> [repeat]
./main --n N --tolerance T --max-iters M --repeat R
```

Defaults match the HeCBench reference: `n = 2048`, `tolerance = 1e-5`, `max-iters = 10000` and
`repeat = 5`. Named options also accept the `--option=value` form. Invalid arguments print a
diagnostic to stderr and exit non-zero.

The program runs one untimed warm-up solve, then `repeat` measured solves. It reports a
`jacobi_result` diagnostic line and a `Total elapsed time: <value> seconds` line that keeps the
upstream output contract.

## Test semantics

`src/helper.hpp` contains an independent double-precision host reference. It re-derives the
sinusoidal boundary from `sin(...)`, applies the five-point average over the full domain and
computes the L2 change directly. It shares no indexing, tiling or reduction helper with the device
kernel, so a wrong halo, boundary update or reduction cannot pass by construction.

`jacobi::verify()` checks that the reported iteration count is positive, that every device value is
finite, and that the final field matches the double reference computed to the same iteration.
Because the device result is `float` and the reference is `double`, the comparison uses a small
documented tolerance rather than bit equality. A failed check exits non-zero.

CTest registers `jacobi-alpaka` with the upstream `Total elapsed time` regex and a small
convergence case (`--n 64 --max-iters 2000 --repeat 2`) so the test fits a CI timeout. The vendored
HeCBench tools still use the arguments recorded for `jacobi` in `subset.json`. The test passes only
when the benchmark exits zero and prints a matching numeric value.

## Provenance

- HeCBench reference pinned at commit `0a17ee47ca6072192101036d4bf292e6612f22e6`:
  `src/jacobi-cuda/main.cu` and `src/jacobi-omp/main.cpp`.
- Original alpaka port: alpaka3 PR #43 head `4d5dfffb65e5`, `benchmark/jacobi`.
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the
  `extern/alpaka` submodule.
- The benchmark logic is re-derived, not vendored. The source files carry the upstream
  `BSD-3-Clause` license notice.

## Build and run

CPU serial executor:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target jacobi-alpaka -j
./build/cpu-serial/bin/alpaka/jacobi --n 64 --max-iters 2000 --repeat 2
```

CPU with the OpenMP block executor:

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target jacobi-alpaka -j
./build/cpu/bin/alpaka/jacobi --n 64 --max-iters 2000 --repeat 2
```

CUDA (default `sm_80`, override with `-DCMAKE_CUDA_ARCHITECTURES=<n>`):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target jacobi-alpaka -j
./build/cuda/bin/alpaka/jacobi --n 2048 --repeat 5
```

The `make` wrapper configures a Release build, builds `jacobi-alpaka` and copies
`bin/alpaka/jacobi` to `./main` for the HeCBench tooling:

```bash
make -C src/jacobi-alpaka
python3 tools/hecbench run jacobi --model alpaka
```

## Run the test

```bash
ctest --test-dir build/cpu-serial -R jacobi-alpaka --output-on-failure
```

## License

BSD-3-Clause. See the repository `LICENSE` and `REUSE.toml`.
