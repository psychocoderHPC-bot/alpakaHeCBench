# heat2d performance on CUDA, AMD HIP and Intel SYCL

Measured 2026-10-19 on the shared `hal` host (AMD EPYC 7452). The current 2-D
work division is faster than the previous 1-D kernel on the A30 and the Arc
A770. It is slower on the RX 7900 XTX, where the 1-D kernel remains the fastest
form. On every backend and size the current kernel beats the ORNL reference.

![heat2d performance](heat2d-performance.png)

Lower is better in the top row. The bottom row shows paired `% time change` of
the current 2-D kernel. Negative values mean it is faster. Repository path:
`src/heat2d-alpaka/docs/heat2d-performance.png`.

## Method

The two alpaka variants share one build tree, compiler flags, Release
configuration, `-O3 -DNDEBUG -std=c++20` and fast math off. Only
`src/heat2d-alpaka/main.cpp` differs between them. All three variants use FP32
device arithmetic. ORNL is built separately from its own source. The timed
region covers the `niter` kernel launches submitted back to back on a
non-blocking queue plus one final synchronization. Host transfer, host
verification and buffer initialization sit outside the timer. `usec/iter` is the
timed region divided by `niter`.

| Backend | Device | Target | Toolchain | Driver / runtime |
|---|---|---|---|---|
| CUDA | NVIDIA A30 24 GiB | `sm_80` | CUDA 13.4, nvcc V13.4.92 | 610.57.04 |
| AMD HIP | Radeon RX 7900 XTX | `gfx1100` | ROCm 7.2, clang++ from `/opt/rocm/llvm` | ROCm 7.2 |
| Intel SYCL | Arc A770 (DG2) | `spir64` | oneAPI 2026.2, icpx, Level Zero | oneAPI 2026.2 |

Sizes are `Lx x Ly x iterations`: `1024 x 1024 x 100` and `4096 x 4096 x 100`.
CUDA used 20 paired rounds at 1024 and 8 at 4096. The AMD and Intel campaigns
used 15 and 8. Each measurement starts a fresh process. Variant order rotates
between rounds, and analysis pairs measurements by round. Warm-up runs are
discarded. Every measured run passed verification with exit code 0.

The port compares the device field with an independent double-precision host
reference under `1e-5 + 1e-7*niter`. ORNL checks float against float at `1e-2`.
ORNL also leaves malloc cells uninitialised while the port zero-fills them.
Neither the reference construction nor the check is inside the timed region.

## Results

Medians in microseconds per iteration. Change is `(T_current/T_ref - 1) * 100`;
negative means the current 2-D kernel is faster. Speedup is `T_ref/T_current`.

| Backend | Size | prev 1-D | current 2-D | ORNL | vs prev 1-D | vs ORNL |
|---|---|---:|---:|---:|---|---|
| CUDA A30 | 1024x1024x100 | 27.655 | 24.257 | 29.800 | -12.25%, 1.14x | -18.60%, 1.23x |
| CUDA A30 | 4096x4096x100 | 382.729 | 329.537 | 399.200 | -13.90%, 1.16x | -17.44%, 1.21x |
| AMD RX 7900 XTX | 1024x1024x100 | 22.273 | 29.880 | 42.100 | +34.32%, 0.74x | -29.27%, 1.41x |
| AMD RX 7900 XTX | 4096x4096x100 | 175.729 | 196.335 | 257.450 | +11.66%, 0.90x | -23.75%, 1.31x |
| Intel Arc A770 | 1024x1024x100 | 54.164 | 51.539 | 194.700 | -4.62%, 1.05x | -73.46%, 3.77x |
| Intel Arc A770 | 4096x4096x100 | 544.936 | 485.256 | 552.900 | -11.10%, 1.13x | -12.50%, 1.14x |

Paired per-round ratios. The confidence interval is a percentile bootstrap over
rounds (10000 resamples). The p-value is a two-sided exact sign test. Ratios
below 1 mean the current 2-D kernel is faster.

| Backend | Size | Comparison | n | Median ratio | % time change [95% CI] | Speedup | Sign p |
|---|---|---|---:|---:|---|---:|---:|
| CUDA | 1024 | current/prev 1-D | 20 | 0.8775 | -12.25 [-13.64, -12.22] | 1.14x | 4.0e-05 |
| CUDA | 1024 | current/ORNL | 20 | 0.8140 | -18.60 [-20.19, -18.56] | 1.23x | 4.0e-05 |
| CUDA | 4096 | current/prev 1-D | 8 | 0.8610 | -13.90 [-13.90, -11.10] | 1.16x | 7.8e-03 |
| CUDA | 4096 | current/ORNL | 8 | 0.8256 | -17.44 [-24.24, -15.99] | 1.21x | 7.8e-03 |
| AMD | 1024 | current/prev 1-D | 15 | 1.3432 | +34.32 [23.23, 41.16] | 0.74x | 6.1e-05 |
| AMD | 1024 | current/ORNL | 15 | 0.7073 | -29.27 [-30.22, -28.77] | 1.41x | 6.1e-05 |
| AMD | 4096 | current/prev 1-D | 8 | 1.1166 | +11.66 [11.19, 12.02] | 0.90x | 7.8e-03 |
| AMD | 4096 | current/ORNL | 8 | 0.7625 | -23.75 [-24.52, -23.24] | 1.31x | 7.8e-03 |
| Intel | 1024 | current/prev 1-D | 15 | 0.9538 | -4.62 [-5.38, -1.74] | 1.05x | 9.8e-04 |
| Intel | 1024 | current/ORNL | 15 | 0.2654 | -73.46 [-75.58, -72.27] | 3.77x | 6.1e-05 |
| Intel | 4096 | current/prev 1-D | 8 | 0.8890 | -11.10 [-11.20, -10.80] | 1.13x | 7.8e-03 |
| Intel | 4096 | current/ORNL | 8 | 0.8750 | -12.50 [-13.32, -11.75] | 1.14x | 7.8e-03 |

The bootstrap uses seed 20261019. At 1024 on AMD the lower bound moves between
1.23 and 1.31 across seeds because the ratio distribution is skewed; the
median ratio and the 15/15 sign test stay fixed.

## AMD RDNA3 regression

The 2-D work division loses 34% against the previous 1-D kernel at 1024 and 12%
at 4096. A separate shape isolation campaign (C1) tested 16x16, 1x256, 8x32 and
32x8 frames. All landed within ~2 percent of the 16x16 frame, so block shape
does not explain the gap. A hybrid variant that keeps the 2-D frame and switches
to flat addressing (H1) recovered part of the loss: +7.5% and +7.7% versus the
1-D kernel at 1024 and 4096. The 1-D kernel remains the fastest form on this
GPU. The current 2-D form still beats ORNL HIP by 1.41x at 1024 and 1.31x at
4096.

## Caveats

- `hal` is shared and non-exclusive. A30 SM clock varied from 930 to 1440 MHz.
  AMD and Intel clocks were not forced. Interleaving and per-round pairing
  reduce clock drift. Treat single runs as unreliable.
- Absolute times are not comparable across vendors. Only compare deltas within
  one backend.
- On AMD RDNA3 the 2-D form loses 34% at 1024 and 12% at 4096 against the prev
  1-D kernel. No tested 2-D frame shape recovers the loss. The 1-D kernel stays
  fastest. The 2-D form still beats ORNL HIP by 1.41x and 1.31x.
- ORNL leaves malloc cells uninitialised while the port zero-fills them.
  Verification tolerances differ: ORNL uses 1e-2 float against float, the port
  uses `1e-5 + 1e-7*niter` float against double. Neither step is timed.
- The Arc A770 is FP32-only and linked at PCIe x1. Host transfer is outside the
  timer.
- The CUDA 4096x4096x1000 case used 5 rounds. Its ratio is 0.895 with CI
  [0.885, 1.095], so the interval crosses 1 and the sign test gives p=0.375. The
  direction matches the 100-iteration result. Treat it as directional only, and
  note that it stays out of the tables above. Current versus ORNL at that size
  gives 0.747 [0.678, 0.878], sign p=0.063.

Raw round data and the plotting script are kept with the benchmark artifacts.
The repository ships the figure and this report only.
