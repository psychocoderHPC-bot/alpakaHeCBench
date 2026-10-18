<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# ising (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of
the HeCBench `ising` benchmark. One C++20 translation unit runs a 2D
square-lattice Ising Monte-Carlo simulation with a checkerboard update and is
compiled unchanged for the host CPU and for GPU backends. There is no backend
branch and no native CUDA/HIP/OpenMP kernel.

## Computation

The lattice has `nx` rows and `ny` columns with periodic boundaries. Spins are
split by `(row + column) % 2` into a black and a white checkerboard color. Each
color is stored row-wise as a packed `nx * (ny / 2)` array of `signed char`
values `+1` or `-1`. One full update performs two passes: the black cells are
updated from the frozen white lattice, then the white cells from the updated
black lattice. Within a color every cell reads only the opposite color and
itself, so the parallel update is race free.

For a cell with spin `s` and nearest-neighbor sum `m`, the flip is accepted when

```text
randval < exp(-2 * beta * m * s),    beta = 1 / (alpha * TCRIT)
```

with `TCRIT = 2.26918531421f` and `alpha` defaulting to `0.1`. The off-column
neighbor follows the HeCBench `update_lattice` rule
`joff = is_black ? (row_odd ? j+1 : j-1) : (row_odd ? j-1 : j+1)`. The random
buffer of `nx * (ny / 2)` floats is generated once with `srand(seed)` and
`rand() / RAND_MAX` and reused for every value and iteration, matching the
reference `#ifndef CURAND` verification path.

Each measured repetition reinitializes the lattice, runs the untimed warm-up,
then times `niters` full updates. The reported value is the median over
`repeat` samples and is emitted both as a runtime and as the upstream-compatible
`elapsed time: <value> sec` token. The `updates per ns` value uses the reference
formula `nx * ny * niters / duration`.

## Command line

The reference accepts named options only. This port keeps the same names,
defaults and validation:

```text
./ising [--lattice-n <nx>] [--lattice-m <ny>] [--nwarmup <n>]
        [--niters <n>] [--repeat <n>] [--alpha <a>] [--seed <s>]
```

`nx` and `ny` default to `5120`, `nwarmup` to `100`, `niters` to `1000`,
`repeat` to `5`, `alpha` to `0.1` and `seed` to `1234`. Options also accept the
`--name=value` spelling. Dimensions must be positive and even; `nwarmup` must be
non-negative, `niters`, `repeat` and `alpha` positive. Invalid or unknown
arguments and unexpected positional arguments exit non-zero. `-h`/`--help`
prints the usage text.

## Test semantics

`helper.hpp` holds an independent host reference. It stores the full `nx x ny`
lattice and updates one color at a time in place, reading the frozen opposite
color and using double-precision `std::exp`. The kernel uses the packed
black/white layout and single-precision arithmetic, so the reference does not
reuse the kernel's indexing or arithmetic. Both consume the same `srand(seed)`
stream. The comparison against the final packed buffers is exact because the
state is discrete `{-1, +1}`, as in the HeCBench reference. A domain check
rejects any entry outside `{-1, +1}`. Any mismatch or invalid entry makes the
process exit non-zero.

The registered CTest case runs the small lattice
`--lattice-n 64 --lattice-m 64 --nwarmup 10 --niters 50 --repeat 1`, requires
the `elapsed time` line and passes only if verification succeeds. The upstream
`subset.json` entry for `ising` supplies the same regex and no arguments; the
reference harness therefore runs the full defaults, which are too large for a
serial CPU test.

## Provenance

The implementation is a faithful adaptation of HeCBench at commit
[`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/tree/0a17ee47ca6072192101036d4bf292e6612f22e6):

- `src/ising-cuda/main.cu`
- `src/ising-cuda/reference.h`
- `src/ising-omp/main.cpp`

The referenced CUDA and OpenMP sources are not vendored here; only the alpaka
implementation is committed. The HeCBench sources carry the NVIDIA 2019
MIT-style license. The original benchmark derives from the NVIDIA Ising sample.

## Build and run

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target ising-alpaka -j
./build/cpu-serial/bin/alpaka/ising --lattice-n 1024 --lattice-m 1024 --nwarmup 10 --niters 200 --repeat 5
ctest --test-dir build/cpu-serial -R ising-alpaka --output-on-failure
```

The `cuda` preset builds the same source for CUDA. The default target
architecture is `sm_80`; set `-DCMAKE_CUDA_ARCHITECTURES=<n>` for another GPU.
`ising-alpaka` runs every enabled backend and executor and reports each one
separately.

## License

The benchmark implementation is BSD-3-Clause. The independent reference in
`helper.hpp` contains no HeCBench code and uses the same license. See the
top-level `LICENSE` and `REUSE.toml`.
