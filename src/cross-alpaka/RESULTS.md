<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# cross (alpaka) results: cross-opt campaign2

This report packages the corrected cross-opt campaign2 measurements for the alpaka `cross`
benchmark against the ORNL/HeCBench `cross` references. Run id `cross-opt-20261011`.

The PR ships the unchanged baseline `src/cross-alpaka/main.cpp` (C1). It wins the primary
geomean at 0.886 over three portable alternatives, all rejected. The ORNL reference stays
faster on CPU (C1 geomean 0.83) and Intel (0.96), and reaches near parity on AMD (0.91). No
portable variant wins everywhere, so this is a "least regression / best port" choice. The
selection reasoning is in the Selection section and summarized in the table below it.

Every candidate number here comes from `results/data/campaign2.csv` (348 rows). The
selected-implementation numbers come from `results/data/final_selected.csv`, the
hal-validated re-run of the shipped presets (see the next section). The older `campaign.csv`
is superseded: it carried an Intel harness argument bug and no in-process warm-up. The
intervening harness and warm-up fixes are recorded verbatim in
`results/data/campaign2_applied_diffs.diff`.

The reconfigured final branch overlays the hal-validated build and measurement artifacts:
`scripts/measure.py` (the re-run harness, with the OMP_NUM_THREADS command prefix and a
sample stddev), and `results/data/final_selected.csv` and
`results/data/one_binary_per_target.txt`. `CMakePresets.json` and the other `scripts/` are
byte-identical between the two source trees. The doc prose in this file is unchanged except
for the numbers the re-run contradicts.

Raw data and the analysis script:

- `results/data/campaign2.csv` - authoritative 13-column measurements.
- `results/data/campaign2_cross3_table.csv` - the per-case table below, one role row per impl.
- `results/data/campaign2_stability.csv` - repeat-to-repeat noise per (impl, case).
- `results/data/campaign2_analysis.txt` - textual roll-up and geomean ranking.
- `results/data/campaign2_exact_commands.log` - the exact command line per measured case.
- `results/data/campaign2_applied_diffs.diff` - F1 (harness) and F2 (warm-up) fixes.
- `results/data/final_selected.csv` - hal-validated re-run of the selected C1 binary
  (`cpu-icpx`, `hip`, `sycl`), 13-column schema, same sizes/dtypes as campaign2.
- `results/data/one_binary_per_target.txt` - `sha256`, `ldd` and `.comment` provenance for
  the three selected-implementation binaries, the one-binary-per-target evidence.
- `results/summary.md`, `results/ranking.md`, `results/summary.json` - generated reports.
- `results/*.png` - runtime and speedup plots per architecture.

## Hal-validated selected-implementation re-run

The three presets (`cpu-icpx`, `hip`, `sycl`) were configured and built on the campaign
host, and the resulting selected-implementation (C1) binaries were re-measured into
`results/data/final_selected.csv`. This bundle is the build and measurement artifact of
record; where it disagrees with the campaign2 per-case table, this table wins because it
was produced from the shipped presets and scripts.

The re-run confirms the campaign2 selection decisively: primary geomean 0.8863 over the
same 20 primary cases, against 0.8864 from campaign2. The per-architecture geomeans are
CPU 0.813, AMD 0.921, Intel 0.978, against campaign2 CPU 0.831, AMD 0.908, Intel 0.961.
The differences sit inside the measurement-noise band and do not change any ranking or the
selection.

Four baseline cells move by more than 5% between the two runs:

| arch | dtype | size | campaign2 median_us | re-run median_us | move |
| --- | --- | ---: | ---: | ---: | ---: |
| CPU | FP64 | 100000 | 80.950 | 96.089 | +18.7% |
| AMD GPU | FP64 | 1000000 | 40.921 | 38.587 | -5.7% |
| Intel GPU | FP32 | 100000 | 38.385 | 31.956 | -16.7% |
| Intel GPU | FP32 | 1000000 | 97.674 | 109.540 | +12.1% |

The CPU FP64 100K cell is the campaign2 worst case; the re-run confirms it is the worst
cell at speedup 0.225 (campaign2 0.267). All four moves are noise-band effects on a shared
host.

## Fairness protocol

The reference is ORNL/HeCBench `cross` at commit
[`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/commit/0a17ee47ca6072192101036d4bf292e6612f22e6).
The comparison follows `evidence/fairness.md`:

- **No fast math.** ORNL builds use plain `-O3`. No `-ffast-math` or `-Ofast` on either
  side; `alpakaHeCBench_FAST_MATH=OFF` in every alpaka build.
- **CPU compiler matched.** The primary CPU pairing is icpx against icpx with
  `-march=native` and `-qopenmp`. ORNL `cross-omp` is an OpenMP-offload kernel
  (`#pragma omp target`, `DEVICE=gpu`): its `-qopenmp` branch lowers the target region to a
  normal threaded region, so both sides run true CPU-threaded kernels with matched compiler
  and flags. `g++` cannot lower the offload region to host threads; libgomp runs it through a
  host fallback that shows a flat ~25 ms per-launch artifact (24956 us at 1e5 and 26870 us at
  1e6, thread-count insensitive), so g++ is not a valid CPU reference. The g++ rows are
  retained below only as a documented secondary.
- **One binary per target.** Each backend is built by its own preset into its own tree and
  links only its own runtime. `scripts/one_binary_per_target.sh` prints the `ldd` and
  `.comment` proof. No binary is reused across backends.
- **Intel Arc A770 has no FP64.** The device does not support the `fp64` aspect. Intel FP64
  is N/A; only FP32 is compared on the A770. CPU and AMD gfx1100 are compared on FP32 and
  FP64.
- **Matched timing scope.** Both sides time `repeat` enqueues followed by one
  synchronization and report the mean. Campaign2 adds one discarded in-process warm-up (F2)
  so the first timed kernel does not absorb one-time first-touch and thread-pool cost. The
  boundary is unchanged.
- **Strict serialization.** One timed process at a time (harness flock). `OMP_NUM_THREADS=64`
  on the CPU. Sizes 100000, 1000000, 10000000, 10000003; repeats 100, 100, 30, 100.

## Measurement environment

All runs came from one shared `hal` node, strictly sequential, with no benchmarks running in
parallel:

- CPU: AMD EPYC 7452 (Zen2, AVX2 and FMA, no AVX-512), 32 cores / 64 threads.
- AMD GPU: Radeon RX 7900 XTX, gfx1100, HIP.
- Intel GPU: Arc A770, SYCL (FP32 only, no FP64).
- Host toolchains: g++ 13.3, icpx 2026.1.1, hipcc 7.2, CMake 3.28.3.

The host is not exclusive. The CPU alone shows a mean stddev/median near 16%, so sub-5% gaps
on CPU, AMD and Intel are not resolvable. See Measurement noise.

## Bottleneck characterization

The kernels are memory-bandwidth bound, not compute bound. The C5 investigation recorded the
operational intensity near 0.42 FLOP/byte and the realized bandwidth at 1e7 AMD (C1 about
728 GB/s, 76% of the ~960 GB/s peak) and CPU (C1 about 87 GB/s, roughly half the L3 triad
rate). The proposed C5 structure (row-chunks over SoA planes) is what C2 and C3 already do,
so it cannot beat the memory wall.

## Kernel-role routing

Candidate role rows are selected by kernel tag, not by a flat `[cross3]` match. Their own
`[cross3]` sibling rows are diagnostics.

| impl | measured role kernel |
| --- | --- |
| baseline (C1) | `cross3` |
| C2 (SoA SimdAlgo) | `cross3` |
| C3 (SoA transform) | `crossSoA` |
| C4 (AoS gather) | `cross4` |
| ORNL references | `cross3` |

## Per-case table

Speedup is ORNL reference divided by the candidate. A value below 1.0 is slower than the
reference. `regression` marks speedup < 1.0. CPU-gcc rows are the non-primary secondary.

### CPU (icpx)

| impl | dtype | size | median_us | ORNL_us | speedup | regression |
| --- | --- | --- | ---: | ---: | ---: | --- |
| baseline-c1-cpu-icpx | FP32 | 100000 | 30.813 | 22.676 | 0.736 | yes |
| baseline-c1-cpu-icpx | FP32 | 1000000 | 66.339 | 79.714 | 1.202 |  |
| baseline-c1-cpu-icpx | FP32 | 10000000 | 4156.109 | 4172.681 | 1.004 |  |
| baseline-c1-cpu-icpx | FP32 | 10000003 | 4202.010 | 4185.260 | 0.996 | yes |
| baseline-c1-cpu-icpx | FP64 | 100000 | 80.950 | 21.610 | 0.267 | yes |
| baseline-c1-cpu-icpx | FP64 | 1000000 | 84.367 | 82.197 | 0.974 | yes |
| baseline-c1-cpu-icpx | FP64 | 10000000 | 8416.069 | 8220.940 | 0.977 | yes |
| baseline-c1-cpu-icpx | FP64 | 10000003 | 8262.009 | 8384.381 | 1.015 |  |
| cand-c2-cpu-icpx | FP32 | 100000 | 23.909 | 22.676 | 0.948 | yes |
| cand-c2-cpu-icpx | FP32 | 1000000 | 64.360 | 79.714 | 1.239 |  |
| cand-c2-cpu-icpx | FP32 | 10000000 | 4151.627 | 4172.681 | 1.005 |  |
| cand-c2-cpu-icpx | FP32 | 10000003 | 4199.550 | 4185.260 | 0.997 | yes |
| cand-c2-cpu-icpx | FP64 | 100000 | 70.757 | 21.610 | 0.305 | yes |
| cand-c2-cpu-icpx | FP64 | 1000000 | 88.463 | 82.197 | 0.929 | yes |
| cand-c2-cpu-icpx | FP64 | 10000000 | 8254.508 | 8220.940 | 0.996 | yes |
| cand-c2-cpu-icpx | FP64 | 10000003 | 8292.396 | 8384.381 | 1.011 |  |
| cand-c3-cpu-icpx | FP32 | 100000 | 66.487 | 22.676 | 0.341 | yes |
| cand-c3-cpu-icpx | FP32 | 1000000 | 111.267 | 79.714 | 0.716 | yes |
| cand-c3-cpu-icpx | FP32 | 10000000 | 6255.203 | 4172.681 | 0.667 | yes |
| cand-c3-cpu-icpx | FP32 | 10000003 | 6350.517 | 4185.260 | 0.659 | yes |
| cand-c3-cpu-icpx | FP64 | 100000 | 316.152 | 21.610 | 0.068 | yes |
| cand-c3-cpu-icpx | FP64 | 1000000 | 188.957 | 82.197 | 0.435 | yes |
| cand-c3-cpu-icpx | FP64 | 10000000 | 13038.777 | 8220.940 | 0.630 | yes |
| cand-c3-cpu-icpx | FP64 | 10000003 | 12974.714 | 8384.381 | 0.646 | yes |
| cand-c4-cpu-icpx | FP32 | 100000 | 521.615 | 22.676 | 0.043 | yes |
| cand-c4-cpu-icpx | FP32 | 1000000 | 708.173 | 79.714 | 0.113 | yes |
| cand-c4-cpu-icpx | FP32 | 10000000 | 4523.933 | 4172.681 | 0.922 | yes |
| cand-c4-cpu-icpx | FP32 | 10000003 | 4639.793 | 4185.260 | 0.902 | yes |
| cand-c4-cpu-icpx | FP64 | 100000 | 207.693 | 21.610 | 0.104 | yes |
| cand-c4-cpu-icpx | FP64 | 1000000 | 246.472 | 82.197 | 0.333 | yes |
| cand-c4-cpu-icpx | FP64 | 10000000 | 8336.053 | 8220.940 | 0.986 | yes |
| cand-c4-cpu-icpx | FP64 | 10000003 | 8424.140 | 8384.381 | 0.995 | yes |
| ornl-cross-omp-cpu-icpx | FP32 | 100000 | 22.676 | 22.676 | 1.000 |  |
| ornl-cross-omp-cpu-icpx | FP32 | 1000000 | 79.714 | 79.714 | 1.000 |  |
| ornl-cross-omp-cpu-icpx | FP32 | 10000000 | 4172.681 | 4172.681 | 1.000 |  |
| ornl-cross-omp-cpu-icpx | FP32 | 10000003 | 4185.260 | 4185.260 | 1.000 |  |
| ornl-cross-omp-cpu-icpx | FP64 | 100000 | 21.610 | 21.610 | 1.000 |  |
| ornl-cross-omp-cpu-icpx | FP64 | 1000000 | 82.197 | 82.197 | 1.000 |  |
| ornl-cross-omp-cpu-icpx | FP64 | 10000000 | 8220.940 | 8220.940 | 1.000 |  |
| ornl-cross-omp-cpu-icpx | FP64 | 10000003 | 8384.381 | 8384.381 | 1.000 |  |

### AMD GPU (gfx1100)

| impl | dtype | size | median_us | ORNL_us | speedup | regression |
| --- | --- | --- | ---: | ---: | ---: | --- |
| baseline-c1-hip | FP32 | 100000 | 10.798 | 7.209 | 0.668 | yes |
| baseline-c1-hip | FP32 | 1000000 | 19.306 | 20.652 | 1.070 |  |
| baseline-c1-hip | FP32 | 10000000 | 494.617 | 455.336 | 0.921 | yes |
| baseline-c1-hip | FP32 | 10000003 | 493.304 | 453.887 | 0.920 | yes |
| baseline-c1-hip | FP64 | 100000 | 12.545 | 10.150 | 0.809 | yes |
| baseline-c1-hip | FP64 | 1000000 | 40.921 | 41.306 | 1.009 |  |
| baseline-c1-hip | FP64 | 10000000 | 957.475 | 925.307 | 0.966 | yes |
| baseline-c1-hip | FP64 | 10000003 | 957.659 | 924.401 | 0.965 | yes |
| cand-c2-hip | FP32 | 100000 | 11.578 | 7.209 | 0.623 | yes |
| cand-c2-hip | FP32 | 1000000 | 34.470 | 20.652 | 0.599 | yes |
| cand-c2-hip | FP32 | 10000000 | 562.321 | 455.336 | 0.810 | yes |
| cand-c2-hip | FP32 | 10000003 | 559.719 | 453.887 | 0.811 | yes |
| cand-c2-hip | FP64 | 100000 | 19.592 | 10.150 | 0.518 | yes |
| cand-c2-hip | FP64 | 1000000 | 42.949 | 41.306 | 0.962 | yes |
| cand-c2-hip | FP64 | 10000000 | 987.139 | 925.307 | 0.937 | yes |
| cand-c2-hip | FP64 | 10000003 | 984.061 | 924.401 | 0.939 | yes |
| cand-c3-hip | FP32 | 100000 | 22.258 | 7.209 | 0.324 | yes |
| cand-c3-hip | FP32 | 1000000 | 44.230 | 20.652 | 0.467 | yes |
| cand-c3-hip | FP32 | 10000000 | 833.074 | 455.336 | 0.547 | yes |
| cand-c3-hip | FP32 | 10000003 | 847.134 | 453.887 | 0.536 | yes |
| cand-c3-hip | FP64 | 100000 | 26.858 | 10.150 | 0.378 | yes |
| cand-c3-hip | FP64 | 1000000 | 72.909 | 41.306 | 0.567 | yes |
| cand-c3-hip | FP64 | 10000000 | 1631.648 | 925.307 | 0.567 | yes |
| cand-c3-hip | FP64 | 10000003 | 1666.652 | 924.401 | 0.555 | yes |
| cand-c4-hip | FP32 | 100000 | 23.596 | 7.209 | 0.306 | yes |
| cand-c4-hip | FP32 | 1000000 | 52.345 | 20.652 | 0.395 | yes |
| cand-c4-hip | FP32 | 10000000 | 494.657 | 455.336 | 0.921 | yes |
| cand-c4-hip | FP32 | 10000003 | 493.696 | 453.887 | 0.919 | yes |
| cand-c4-hip | FP64 | 100000 | 36.991 | 10.150 | 0.274 | yes |
| cand-c4-hip | FP64 | 1000000 | 99.521 | 41.306 | 0.415 | yes |
| cand-c4-hip | FP64 | 10000000 | 1051.162 | 925.307 | 0.880 | yes |
| cand-c4-hip | FP64 | 10000003 | 1039.945 | 924.401 | 0.889 | yes |
| ornl-cross-hip | FP32 | 100000 | 7.209 | 7.209 | 1.000 |  |
| ornl-cross-hip | FP32 | 1000000 | 20.652 | 20.652 | 1.000 |  |
| ornl-cross-hip | FP32 | 10000000 | 455.336 | 455.336 | 1.000 |  |
| ornl-cross-hip | FP32 | 10000003 | 453.887 | 453.887 | 1.000 |  |
| ornl-cross-hip | FP64 | 100000 | 10.150 | 10.150 | 1.000 |  |
| ornl-cross-hip | FP64 | 1000000 | 41.306 | 41.306 | 1.000 |  |
| ornl-cross-hip | FP64 | 10000000 | 925.307 | 925.307 | 1.000 |  |
| ornl-cross-hip | FP64 | 10000003 | 924.401 | 924.401 | 1.000 |  |

### Intel GPU (Arc A770, FP32 only)

| impl | dtype | size | median_us | ORNL_us | speedup | regression |
| --- | --- | --- | ---: | ---: | ---: | --- |
| baseline-c1-sycl | FP32 | 100000 | 38.385 | 27.433 | 0.715 | yes |
| baseline-c1-sycl | FP32 | 1000000 | 97.674 | 138.405 | 1.417 |  |
| baseline-c1-sycl | FP32 | 10000000 | 894.431 | 822.623 | 0.920 | yes |
| baseline-c1-sycl | FP32 | 10000003 | 893.377 | 817.948 | 0.916 | yes |
| cand-c2-sycl | FP32 | 100000 | 113.651 | 27.433 | 0.241 | yes |
| cand-c2-sycl | FP32 | 1000000 | 266.942 | 138.405 | 0.518 | yes |
| cand-c2-sycl | FP32 | 10000000 | 1308.933 | 822.623 | 0.628 | yes |
| cand-c2-sycl | FP32 | 10000003 | 1131.808 | 817.948 | 0.723 | yes |
| cand-c3-sycl | FP32 | 100000 | 69.764 | 27.433 | 0.393 | yes |
| cand-c3-sycl | FP32 | 1000000 | 180.382 | 138.405 | 0.767 | yes |
| cand-c3-sycl | FP32 | 10000000 | 1507.504 | 822.623 | 0.546 | yes |
| cand-c3-sycl | FP32 | 10000003 | 1474.664 | 817.948 | 0.555 | yes |
| cand-c4-sycl | FP32 | 100000 | 374.306 | 27.433 | 0.073 | yes |
| cand-c4-sycl | FP32 | 1000000 | 585.715 | 138.405 | 0.236 | yes |
| cand-c4-sycl | FP32 | 10000000 | 6617.034 | 822.623 | 0.124 | yes |
| cand-c4-sycl | FP32 | 10000003 | 6607.750 | 817.948 | 0.124 | yes |
| ornl-cross-sycl-fp32 | FP32 | 100000 | 27.433 | 27.433 | 1.000 |  |
| ornl-cross-sycl-fp32 | FP32 | 1000000 | 138.405 | 138.405 | 1.000 |  |
| ornl-cross-sycl-fp32 | FP32 | 10000000 | 822.623 | 822.623 | 1.000 |  |
| ornl-cross-sycl-fp32 | FP32 | 10000003 | 817.948 | 817.948 | 1.000 |  |

### CPU (g++ toolchain experiment, non-primary)

| impl | dtype | size | median_us | ORNL_us | speedup | regression |
| --- | --- | --- | ---: | ---: | ---: | --- |
| baseline-c1-cpu-gcc | FP32 | 100000 | 6029.967 | 24956.410 | 4.139 |  |
| baseline-c1-cpu-gcc | FP32 | 1000000 | 89.782 | 26870.357 | 299.283 |  |
| baseline-c1-cpu-gcc | FP32 | 10000000 | 9069.234 | 33279.883 | 3.670 |  |
| baseline-c1-cpu-gcc | FP32 | 10000003 | 9021.101 | 32787.258 | 3.635 |  |
| baseline-c1-cpu-gcc | FP64 | 100000 | 6070.025 | 25234.135 | 4.157 |  |
| baseline-c1-cpu-gcc | FP64 | 1000000 | 121.548 | 29273.740 | 240.841 |  |
| baseline-c1-cpu-gcc | FP64 | 10000000 | 12245.200 | 36836.582 | 3.008 |  |
| baseline-c1-cpu-gcc | FP64 | 10000003 | 12209.770 | 40198.602 | 3.292 |  |
| ornl-cross-omp-cpu-gcc | FP32 | 100000 | 24956.410 | 24956.410 | 1.000 |  |
| ornl-cross-omp-cpu-gcc | FP32 | 1000000 | 26870.357 | 26870.357 | 1.000 |  |
| ornl-cross-omp-cpu-gcc | FP32 | 10000000 | 33279.883 | 33279.883 | 1.000 |  |
| ornl-cross-omp-cpu-gcc | FP32 | 10000003 | 32787.258 | 32787.258 | 1.000 |  |
| ornl-cross-omp-cpu-gcc | FP64 | 100000 | 25234.135 | 25234.135 | 1.000 |  |
| ornl-cross-omp-cpu-gcc | FP64 | 1000000 | 29273.740 | 29273.740 | 1.000 |  |
| ornl-cross-omp-cpu-gcc | FP64 | 10000000 | 36836.582 | 36836.582 | 1.000 |  |
| ornl-cross-omp-cpu-gcc | FP64 | 10000003 | 40198.602 | 40198.602 | 1.000 |  |

## Geomean ranking (primary)

Selection metric: geometric mean of speedup over all equally-weighted
(architecture, size, dtype) cases on the primary architectures (CPU-icpx, AMD GPU,
Intel GPU FP32). Source: `results/ranking.md`, `results/summary.json`.

| rank | candidate | cases | geomean | worst | worst case | regressions |
| --- | --- | --- | ---: | ---: | --- | ---: |
| 1 | baseline C1 | 20 | 0.8864 | 0.2670 | CPU FP64 100K | 14 |
| 2 | C2 SoA SimdAlgo | 20 | 0.7339 | 0.2414 | Intel-GPU FP32 100K | 17 |
| 3 | C3 SoA transform | 20 | 0.4759 | 0.0684 | CPU FP64 100K | 20 |
| 4 | C4 AoS gather | 20 | 0.3360 | 0.0435 | CPU FP32 100K | 20 |

All four candidates sit below 1.0 against the ORNL reference on the primary metric. The
baseline C1 is the least-slow. C2 is the closest candidate.

Worst case overall: C4 on CPU FP32 at 100K, speedup 0.043.

### Secondary ranking (includes the CPU-g++ toolchain experiment)

| rank | candidate | cases | geomean | worst | worst case | regressions |
| --- | --- | --- | ---: | ---: | --- | ---: |
| 1 | baseline C1 | 28 | 1.8028 | 0.2670 | CPU FP64 100K | 14 |
| 2 | C2 SoA SimdAlgo | 20 | 0.7339 | 0.2414 | Intel-GPU FP32 100K | 17 |
| 3 | C3 SoA transform | 20 | 0.4759 | 0.0684 | CPU FP64 100K | 20 |
| 4 | C4 AoS gather | 20 | 0.3360 | 0.0435 | CPU FP32 100K | 20 |

The CPU-g++ baseline geomean (10.6) is the offload-overhead artifact described in the
fairness section; it is not a performance statement.

## Achieved throughput and bandwidth

Traffic model: 36 B/row FP32 and 72 B/row FP64 (a 3-component row read twice and written
once). C3 reads the same logical 3-vectors from planar arrays, so the per-row byte count is
unchanged; applying 36/72 B per row to C3 is an assumption, not a measurement.

Ceilings on the campaign host: CPU ~170 GB/s L3 and ~64 GB/s DRAM (EPYC 7452), AMD gfx1100
~960 GB/s HBM (RX 7900 XTX), Intel Arc A770 ~560 GB/s FP32 only. At 10M rows the AMD
baseline reaches 728 GB/s (76% of peak) and CPU C1 reaches ~87 GB/s (about half the L3
triad rate). The kernels are memory-bandwidth bound, not compute bound.

## Measurement noise

From `results/data/campaign2_stability.csv` (stddev/median):

| arch | n | mean | max |
| --- | ---: | ---: | ---: |
| CPU | 16 | 0.164 | 0.308 |
| AMD GPU | 24 | 0.031 | 0.236 |
| Intel GPU | 12 | 0.019 | 0.063 |

CPU small-size cells are the noisiest. Treat CPU 100K and 1M sub-100us cells as
low-confidence. The host is shared, so the CPU mean stddev/median is about 16%. Gaps below
about 5% on CPU, AMD and Intel are not resolvable from these measurements.

## Candidate comparison

Geomean of median_A / median_B over shared primary cells, overhead-dominated cells dropped.
A value above 1.0 means the row candidate is slower than the column candidate. Source:
`results/summary.json`.

| A \ B | baseline | C2 | C3 | C4 |
| --- | ---: | ---: | ---: | ---: |
| baseline | 1.000 | 0.808 | 0.569 | 0.456 |
| C2 | 1.238 | 1.000 | 0.724 | 0.546 |
| C3 | 1.757 | 1.381 | 1.000 | 0.799 |
| C4 | 2.194 | 1.830 | 1.251 | 1.000 |

On the shared cells the baseline is the fastest, then C2, then C3, then C4.

## Selection

**Selected implementation: the unchanged baseline `src/cross-alpaka/main.cpp` (C1).**

C1 stores the data as an AoS flat `numRows * 3` buffer, indexes it with `makeIdxMap` and
`worker::linearThreadsInGrid`, and runs the `cross1`, `cross2` and `cross3` kernels with
`chunkSize 256`. The PR ships this source unchanged. The campaign built and measured three
portable alternatives against it and rejected all of them.

C1 wins the primary geomean at 0.886 over the 20 equally weighted (architecture, size, dtype)
cases on CPU-icpx, AMD gfx1100 and Intel Arc A770 FP32. No portable variant wins everywhere.
The ORNL reference stays ahead where a portable port would want parity: CPU geomean 0.83,
Intel geomean 0.96, AMD geomean 0.91 (near parity). On AMD C1 lands close to ORNL-HIP overall
(1.07 at 1e6 FP32, 0.92 at 1e7 FP32). The decision is therefore "least regression / best port",
not a speedup win.

The C5 experiment, run on AMD at 1e7, measured the alpaka C1 slightly ahead of ORNL-HIP
(1.08). That result sits inside the AMD noise band, so the campaign2 per-cell value (0.92)
stays the authoritative number here; C5 is recorded as a documented negative result below.

The per-architecture geomeans and the rejection reason for each variant. The C1 row shows
the hal-validated re-run value first, with the campaign2 value in parentheses. C2, C3 and C4
were measured only in campaign2, so their rows have one value.

| candidate | CPU geomean | AMD geomean | Intel geomean | primary geomean | outcome |
| --- | ---: | ---: | ---: | ---: | --- |
| C1 baseline | 0.813 (0.831) | 0.921 (0.908) | 0.978 (0.961) | 0.8863 (0.8864) | selected |
| C2 SoA planes + `SimdAlgo::concurrent` | 0.872 | 0.757 | 0.488 | 0.7339 | rejected |
| C3 SoA planes + 3x `transform` | 0.435 | 0.484 | 0.550 | 0.4759 | rejected |
| C4 AoS + per-worker `Simd<T,4>` gather | 0.329 | 0.556 | 0.128 | 0.3360 | rejected |

Rejection reasons, in one line each:

- **C2** is the closest rival and the best CPU variant (geomean 0.87, 1.24x at 1e6), but it
  loses on the AMD GPU and on Intel. SoA traffic and access cost is structural.
- **C3** regresses everywhere: about 0.48 to 0.55 on GPU and 0.67 on CPU at 1e7.
- **C4** issues real packed SIMD on the CPU (vmulps/FMA), yet loses on every target; its worst
  cell is 0.04.

The full regression list is in `results/ranking.md` and repeated below. The C5 follow-up is a
documented negative result: correcting the C2 frame specification did not change the outcome.

## Candidate comparison by architecture

Source: `results/ranking.md`, `results/summary.json`, `results/data/campaign2_analysis.txt`.
Primary geomean is over all 20 primary cases; the per-architecture columns are geomeans over
that architecture's own cases (CPU 8, AMD 8, Intel 4).

| candidate | CPU cases | CPU geomean | AMD cases | AMD geomean | Intel cases | Intel geomean | primary geomean | regressions |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| C1 baseline | 8 | 0.813 (0.831) | 8 | 0.921 (0.908) | 4 | 0.978 (0.961) | 0.8863 (0.8864) | 14 (14) |
| C2 SoA SimdAlgo | 8 | 0.872 | 8 | 0.757 | 4 | 0.488 | 0.7339 | 17 |
| C3 SoA transform | 8 | 0.435 | 8 | 0.484 | 4 | 0.550 | 0.4759 | 20 |
| C4 AoS gather | 8 | 0.329 | 8 | 0.556 | 4 | 0.128 | 0.3360 | 20 |

## Full regression list (primary cases, speedup < 1.0)

Every primary cell where the candidate is slower than the ORNL reference. Median is the
candidate value in microseconds, ORNL is the reference in microseconds.

### C1 baseline (14)

| arch | dtype | size | median_us | ORNL_us | speedup |
| --- | --- | ---: | ---: | ---: | ---: |
| AMD GPU | FP32 | 100000 | 10.798 | 7.209 | 0.668 |
| AMD GPU | FP32 | 10000000 | 494.617 | 455.336 | 0.921 |
| AMD GPU | FP32 | 10000003 | 493.304 | 453.887 | 0.920 |
| AMD GPU | FP64 | 100000 | 12.545 | 10.150 | 0.809 |
| AMD GPU | FP64 | 10000000 | 957.475 | 925.307 | 0.966 |
| AMD GPU | FP64 | 10000003 | 957.659 | 924.401 | 0.965 |
| CPU | FP32 | 100000 | 30.813 | 22.676 | 0.736 |
| CPU | FP32 | 10000003 | 4202.010 | 4185.260 | 0.996 |
| CPU | FP64 | 100000 | 80.950 | 21.610 | 0.267 |
| CPU | FP64 | 1000000 | 84.367 | 82.197 | 0.974 |
| CPU | FP64 | 10000000 | 8416.069 | 8220.940 | 0.977 |
| Intel GPU | FP32 | 100000 | 38.385 | 27.433 | 0.715 |
| Intel GPU | FP32 | 10000000 | 894.431 | 822.623 | 0.920 |
| Intel GPU | FP32 | 10000003 | 893.377 | 817.948 | 0.916 |

### C2 SoA SimdAlgo (17)

| arch | dtype | size | median_us | ORNL_us | speedup |
| --- | --- | ---: | ---: | ---: | ---: |
| AMD GPU | FP32 | 100000 | 11.578 | 7.209 | 0.623 |
| AMD GPU | FP32 | 1000000 | 34.470 | 20.652 | 0.599 |
| AMD GPU | FP32 | 10000000 | 562.321 | 455.336 | 0.810 |
| AMD GPU | FP32 | 10000003 | 559.719 | 453.887 | 0.811 |
| AMD GPU | FP64 | 100000 | 19.592 | 10.150 | 0.518 |
| AMD GPU | FP64 | 1000000 | 42.949 | 41.306 | 0.962 |
| AMD GPU | FP64 | 10000000 | 987.139 | 925.307 | 0.937 |
| AMD GPU | FP64 | 10000003 | 984.061 | 924.401 | 0.939 |
| CPU | FP32 | 100000 | 23.909 | 22.676 | 0.948 |
| CPU | FP32 | 10000003 | 4199.550 | 4185.260 | 0.997 |
| CPU | FP64 | 100000 | 70.757 | 21.610 | 0.305 |
| CPU | FP64 | 1000000 | 88.463 | 82.197 | 0.929 |
| CPU | FP64 | 10000000 | 8254.508 | 8220.940 | 0.996 |
| Intel GPU | FP32 | 100000 | 113.651 | 27.433 | 0.241 |
| Intel GPU | FP32 | 1000000 | 266.942 | 138.405 | 0.518 |
| Intel GPU | FP32 | 10000000 | 1308.933 | 822.623 | 0.628 |
| Intel GPU | FP32 | 10000003 | 1131.808 | 817.948 | 0.723 |

### C3 SoA transform (20, all primary cells)

| arch | dtype | median/ORNL speedup range |
| --- | --- | --- |
| AMD GPU | FP32 | 0.324 to 0.547 |
| AMD GPU | FP64 | 0.378 to 0.567 |
| CPU | FP32 | 0.341 to 0.716 |
| CPU | FP64 | 0.068 to 0.646 |
| Intel GPU | FP32 | 0.393 to 0.767 |

### C4 AoS gather (20, all primary cells)

| arch | dtype | median/ORNL speedup range |
| --- | --- | --- |
| AMD GPU | FP32 | 0.306 to 0.921 |
| AMD GPU | FP64 | 0.274 to 0.889 |
| CPU | FP32 | 0.043 to 0.922 |
| CPU | FP64 | 0.104 to 0.995 |
| Intel GPU | FP32 | 0.073 to 0.236 |

C3 and C4 regress on every primary cell, so their full per-cell rows stay in
`results/ranking.md` and `results/data/campaign2_cross3_table.csv`.

## C5 follow-up: documented negative result

C5 rebuilt C2 with a floor frame specification instead of the ceil one. The first campaign
found a frame over-subscription bug: the ceil spec drove the SoA SIMD path to a scalar
fallback on the GPU at 1e6 and 1e7. C5 switches the GPU SIMD loop on, and still loses on the
GPU. The verdict is NO-GO. SoA traffic and access cost is structural, so a frame fix cannot
recover it.

C5 also compared the alpaka C1 against ORNL-HIP on AMD at 1e7 and measured C1 about 1.08x
ahead. That gap sits inside the AMD noise band (mean stddev/median 0.031), so it is not a
resolvable win and does not change the selection.

The same investigation falsified two competing explanations:

- C2 does not spill registers (reported spills: 0).
- AoS auto-vectorization does not appear under icpx. ORNL and alpaka both stay scalar on icpx
  for the stride-3 row loop; packing appears only under gcc.

## Methodology correction (campaign1 to campaign2)

Campaign2 supersedes a first campaign that carried two defects. Fixes F1 and F2 are recorded
verbatim in `results/data/campaign2_applied_diffs.diff`.

- **F1 harness argument bug.** The harness passed `--prog-args "--dtype float"` in a way that
  replaced the positional `<size> <repeat>` instead of appending to it, so every Intel row ran
  the default 10,000,000 rows at repeat 100 regardless of the requested size. The corrected
  harness always passes `<size> <repeat>` and appends extra arguments after them.
- **F2 missing warm-up.** The first campaign omitted the in-process warm-up, so the
  single-kernel C2 candidate absorbed a first-enqueue cold start of about 25 ms. Campaign2
  adds one discarded warm-up (F2) before the timed loop.

Both fixes change the Intel floor and the C2 CPU cells; the selection does not change.
