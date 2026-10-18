<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# colorwheel (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of
the HeCBench `colorwheel` benchmark. One C++20 translation unit encodes a 2D
vector field as an RGB colour wheel and is compiled unchanged for the host CPU
and for GPU backends. There is no backend branch and no native CUDA/HIP/OpenMP
kernel.

## Computation

For an image of `size x size` pixels, `half_size = size/2`,
`range = 1.04 * truerange`, each non-axis pixel `(x, y)` encodes the vector

```text
fx = x/half_size * range - range
fy = y/half_size * range - range
nfx = fx/truerange, nfy = fy/truerange
```

through `computeColor(nfx, nfy)`:

- the six ramp segments (`RY=15, YG=6, GC=4, CB=11, BM=13, MR=6`, total
  `MAXCOLS = 55`) are filled exactly as the reference `setcols` sequences;
- `rad = sqrt(nfx^2 + nfy^2)`, `a = atan2(-nfy, -nfx)/pi`,
  `fk = (a+1)/2 * (MAXCOLS-1)`, `k0 = int(fk)`, `k1 = (k0+1) % MAXCOLS`,
  `f = fk - k0`;
- each channel `c` is `col = (1-f)*cw[k0][c]/255 + f*cw[k1][c]/255`; inside the
  unit disc `col = 1 - rad*(1-col)`, outside it `col *= 0.75`; the byte is
  `pix[2-c] = int(255*col)`.

Axis pixels (`x == half_size` or `y == half_size`) stay black, reproducing the
reference's black coordinate axes. The kernel builds the ramp once per block in
shared memory as the reference integer divided by 255 in single precision, which
is exactly the value the reference computes per thread; the per-pixel arithmetic
is otherwise unchanged.

## Command line

The HeCBench positional form is accepted:

```text
colorwheel <range> <size> <repeat>
```

as well as named options:

```text
colorwheel --range R --size N --repeat M --samples S --verify-only
```

Defaults are `range=10`, `size=4096`, `repeat=100` and `samples=5`.
`--verify-only` skips the timing loop. `size >= 3` is accepted, including odd
sizes. `misc.hpp` has no Catch2 dependency; it uses a small `std::string`
parser that keeps the same options, defaults, positional form and validation
errors.

## Test semantics

`helper.hpp` contains an independent double-precision host implementation of the
same mapping. It rebuilds the colour ramp from the six transition lengths and
recomputes every channel in double precision, separately from the device code.
After a fresh kernel run the port copies the device image back and:

- checks every non-axis pixel against the double reference with the reference's
  own absolute per-channel tolerance of `1`;
- rejects non-finite reference values, channels outside `[0,255]`, and axis
  pixels that are not exactly black.

A mismatch exits non-zero. The CTest case runs the benchmark with
`--size 256 --repeat 2 --samples 1` and requires the verification output token.
Upstream HeCBench has no `subset.json` or `benchmarks.yaml` test metadata for
`colorwheel`; the local test invocation and the new `subset.json` entry follow
the source port's test arguments.

## Build and test

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target colorwheel-alpaka -j
ctest --test-dir build/cpu-serial -R colorwheel-alpaka --output-on-failure
```

Run the benchmark directly:

```bash
./build/cpu-serial/bin/alpaka/colorwheel --range 10 --size 4096 --repeat 100
```

The `cpu` and `cuda` presets build the same target. The vendored harness can run
it with `python3 tools/hecbench -v run colorwheel --model alpaka`.

## Provenance

- HeCBench reference at
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/commit/0a17ee47ca6072192101036d4bf292e6612f22e6):
  `src/colorwheel-cuda/main.cu` and `src/colorwheel-omp/main.cpp`.
- Algorithm origin: [Middlebury flow code](https://vision.middlebury.edu/flow/code/flow-code/colorcode.cpp),
  Daniel Scharstein, 2007.
- Original alpaka port: [`944e9e1f5e2d3a38c8df3eee92cd625ea39ae977`](https://github.com/psychocoderHPC-bot/alpaka3/commit/944e9e1f5e2d3a38c8df3eee92cd625ea39ae977),
  `benchmark/colorwheel` (alpaka3 PR #41).
- alpaka pin: [`b7d339d07056a9a2a6c4051cc3927157bc5f0d51`](https://github.com/alpaka-group/alpaka/commit/b7d339d07056a9a2a6c4051cc3927157bc5f0d51).

The implementation and helper are BSD-3-Clause. `misc.hpp` contains no HeCBench
code and is BSD-3-Clause like the rest of the benchmark directory. This branch
carries no result files, measurement data, plots or comparison tables.
