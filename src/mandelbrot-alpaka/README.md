<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# mandelbrot Benchmark

Portable [alpaka](https://github.com/alpaka-group/alpaka) implementation of the
HeCBench `mandelbrot` benchmark. One translation unit (`main.cpp`) is compiled
unchanged for the host CPU and for CUDA GPUs; there is no backend branch and no
native CUDA/HIP/OpenMP kernel.

## What it computes

For every pixel of a `rows x cols` image the kernel evaluates the escape-time
iteration of the Mandelbrot set and stores the iteration count at which the
orbit leaves the disk of radius 2, or `max_iterations` when the point is
classified as interior:

```
for (i, j) in image:
    c = ( scaleRow(i), scaleCol(j) )   // scaleRow: -1.5 .. 0.5, scaleCol: -1.0 .. 1.0
    z = 0
    count = 0
    while count < max_iterations and |z|^2 < 4:
        z = z*z + c
        count++
    output[i*cols + j] = count
```

The float `ComplexF` type, `MandelParameters::point`, `scaleRow` and `scaleCol`
are copied from the HeCBench reference; only the loop that walks the image
differs between the native references, and this port replaces that loop with a
single portable alpaka kernel.

## Parameters and test semantics

The program accepts named options and the upstream positional repetition form:

```
./mandelbrot [<repeat>] [--rows <n>] [--cols <n>] [--max-iter <n>] [--repeat <n>]
```

The defaults are `rows = 1024`, `cols = 1024`, `max_iterations = 1000` and
`repeat = 5`. The upstream harness passes the repetition count as `1000`, which
is also the registered CTest argument. One warm-up enqueue is discarded; the
timed value is the median of the measured samples. The program prints the
upstream-compatible field `Average kernel execution time: <value> ms`; CTest
matches that line. No timing value is asserted.

Validation is performed on the host and fails the process with a non-zero exit
code:

- the faithful float reference shares the iteration body with the kernel;
- an independent double-precision reference re-derives the same sampling grid;
- a deterministic set of deep-interior pixels is recomputed in double precision;
- every output value must be finite and within `[0, max_iterations]`.

The float and double references may classify a small fraction of boundary
pixels differently because the escape-time map is chaotic at the parabolic
boundary. The check allows a fixed relative tolerance, the same value used by
the HeCBench reference `Verify`, and always reports the mismatch counts. The
exact float comparison, the interior spot check and the finite/range guards are
independent hard checks.

## Provenance

Faithful adaptation of HeCBench pinned at
[`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/tree/0a17ee47ca6072192101036d4bf292e6612f22e6),
specifically `src/mandelbrot-cuda` and `src/mandelbrot-omp`. The upstream sources
are not vendored here; the exact file SHAs recorded during reconnaissance are in
`THIRD-PARTY-NOTICES.txt`.

**License / attribution.** The copied parameter and iteration code originates
from HeCBench, Copyright (c) 2019 Intel Corporation, under the MIT license,
which in turn attributes it to the oneAPI-samples `DirectProgramming` examples.
The alpaka adaptation is Copyright 2026 René Widera, under the BSD-3-Clause
license. The copied code is confined to
`helper.hpp`, which carries the SPDX expression `BSD-3-Clause AND MIT` and an
inline MIT notice. The full MIT and BSD-3-Clause texts are in
`THIRD-PARTY-NOTICES.txt`.

## Build and run

Host CPU, serial executor, Release:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target mandelbrot-alpaka -j
./build/cpu-serial/bin/alpaka/mandelbrot --rows 1024 --cols 1024 --max-iter 1000 --repeat 5
```

CUDA (the default target architecture is `sm_80`; set
`-DCMAKE_CUDA_ARCHITECTURES` for another GPU):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target mandelbrot-alpaka -j
./build/cuda/bin/alpaka/mandelbrot 1000
```

The executable runs every enabled backend/executor and reports each one
separately.

## Test

```bash
ctest --test-dir build/cpu-serial -R mandelbrot-alpaka --output-on-failure
```

The upstream harness can run the port through the vendored tooling:

```bash
python3 tools/hecbench -v run mandelbrot --model alpaka
```

`compile` and `run` phases are supported; this repository does not ship measured
results, plots or comparison tables.
