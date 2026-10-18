<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# memcpy benchmark

Portable alpaka port of the HeCBench `memcpy` benchmark. It measures the
bandwidth of byte-wise memory copies between host and device memory. One source
file drives the host CPU backend and every device backend through
`onHost::memcpy`; there is no native CUDA/HIP/OpenMP kernel and no backend
branch.

## What it computes

The pinned HeCBench reference allocates a raw byte buffer per size, fills it,
and times `cudaMemcpyAsync` in the host-to-device and device-to-host directions.
It has no compute kernel and does not check the copied bytes.

This port keeps those two directions and the per-copy timing scope, and adds:

- a device-to-device mode that the reference does not have;
- a byte-exact correctness check on a freshly poisoned destination for every
  mode and size.

For each size and mode the timing loop enqueues `repeat` copies back-to-back
between two synchronizations and divides the wall time by `repeat`. One warm-up
batch of `repeat` copies precedes the measured samples. The output reports
median, minimum, maximum, relative spread, effective bandwidth, and the raw
samples for each `(mode, size)` pair.

There is no floating point payload. Data is opaque bytes (`std::uint8_t`).

## Parameters

- Positional HeCBench form: `memcpy <repeat>`.
- Named form: `memcpy --sizes A,B,C --repeat R --samples S --max-bytes N --mode M`.
- `--sizes` defaults to the 16 HeCBench sizes `size[i] = 1 << (i + 6)`, that is
  64 B through 2 MiB. Values are comma separated byte counts.
- `--repeat` defaults to `8192`, matching the HeCBench `./main 8192` invocation.
- `--samples` defaults to `5` independent measured samples per mode and size.
- `--max-bytes` defaults to 4 GiB. The effective cap is the smaller of this
  value and one third of the device global memory, so a device-to-device test
  can hold two payloads. Larger requests are skipped and reported.
- `--mode` is `h2d`, `d2h`, `d2d`, or `all` (default).
- `--verify-only` runs the correctness checks without the timing loop.
- `-h` / `--help` prints the usage.

## Provenance

- Reference: [ORNL/HeCBench](https://github.com/ORNL/HeCBench) at commit
  `0a17ee47ca6072192101036d4bf292e6612f22e6`.
  - CUDA reference: `src/memcpy-cuda/main.cu` (blob
    `4a9230f9c3cf98854c5eea1d52dcb782ac25cfb4`)
  - OpenMP reference: `src/memcpy-omp/main.cpp` (blob
    `4f8d7f585f7289d26398720d7be57e895b75ba5e`)
- Source port: `psychocoderHPC-bot/alpaka3` PR #29, head
  `7a088175144dc82f065376941aa08f894f5149a6`, `benchmark/memcpy`.
- The benchmark logic is re-derived from the pinned reference. Reference
  sources are not vendored.

The pinned reference uses 16 sizes from 64 B to 2 MiB. Larger sizes are
reachable through `--sizes` and are bounded by `--max-bytes`.

## Build and run

Build from the repository root with the CPU serial preset:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target memcpy-alpaka -j
```

Run the default configuration:

```bash
./build/cpu-serial/bin/alpaka/memcpy
```

Run a short explicit configuration:

```bash
./build/cpu-serial/bin/alpaka/memcpy --sizes 64,1000,65536 --repeat 4 --samples 2
```

The same target builds with the `cuda` preset. The preset defaults to
`sm_80`; pass `-DCMAKE_CUDA_ARCHITECTURES=<n>` to target another GPU.

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target memcpy-alpaka -j
./build/cuda/bin/alpaka/memcpy --repeat 8192 --samples 5
```

## Test semantics

`helper.hpp` defines a deterministic, position-dependent byte pattern. The
source buffer is filled with that pattern once. Before each correctness copy the
destination is poisoned with the bit-wise inverse pattern (`b ^ 0xFF`), then one
real copy is performed, then the destination is read back and compared byte for
byte.

Because the destination starts as the inverse of the expected pattern, a copy
that is skipped, truncated, shifted, or partially applied leaves at least one
poison byte and fails at a well-defined offset. The comparison is exact, with no
tolerance. A nonzero exit code signals failure.

## CTest

The CMake test registration runs a fast subset in all three modes:

```bash
ctest --test-dir build/cpu-serial -R memcpy-alpaka --output-on-failure
```
