# Campaign2 (corrected) - authoritative summary

Source: `../measurements/campaign2.csv` (348 rows). This supersedes the old `campaign.csv`, which had an Intel wrong-size harness bug and no in-process warm-up. Every number below derives from campaign2.csv.

## Kernel-role routing (explicit)

| impl | arch | measured role kernel tag |
| --- | --- | --- |
| baseline-c1 | CPU / CPU-gcc / AMD-GPU / Intel-GPU | cross3 |
| cand-c2 | CPU / AMD-GPU / Intel-GPU | cross3 |
| cand-c3 | CPU / AMD-GPU / Intel-GPU | crossSoA |
| cand-c4 | CPU / AMD-GPU / Intel-GPU | cross4 |
| ornl-* | reference | cross3 |

Candidates are NOT filtered by a flat `[cross3]` tag: C3 uses its `crossSoA` row and C4 its `cross4` row. Their own `cross3` sibling rows (diagnostics) are excluded.

## Measurement noise (from campaign2_stability.csv)

| arch | n | mean stddev/median | max stddev/median |
| --- | --- | --- | --- |
| CPU | 16 | 0.164 | 0.308 |
| AMD-GPU | 24 | 0.031 | 0.236 |
| Intel-GPU | 12 | 0.019 | 0.063 |

## Per-case table (ORNL ref + each candidate)

| arch | dtype | size | candidate | median_us | speedup | stddev/median | ORNL ref us | GB/s | overhead | regression |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AMD-GPU | FP32 | 100K | C2 | 11.578 | 0.623 | 0.003 | 7.209 | 310.93 | no | YES |
| AMD-GPU | FP32 | 100K | C3 | 22.258 | 0.324 | 0.004 | 7.209 | 161.74 | YES | YES |
| AMD-GPU | FP32 | 100K | C4 | 23.596 | 0.306 | 0.005 | 7.209 | 152.57 | no | YES |
| AMD-GPU | FP32 | 100K | baseline | 10.798 | 0.668 | 0.001 | 7.209 | 333.39 | YES | YES |
| AMD-GPU | FP32 | 1M | C2 | 34.470 | 0.599 | 0.084 | 20.652 | 1044.40 | no | YES |
| AMD-GPU | FP32 | 1M | C3 | 44.230 | 0.467 | 0.003 | 20.652 | 813.93 | no | YES |
| AMD-GPU | FP32 | 1M | C4 | 52.345 | 0.395 | 0.005 | 20.652 | 687.75 | no | YES |
| AMD-GPU | FP32 | 1M | baseline | 19.306 | 1.070 | 0.008 | 20.652 | 1864.74 | no | no |
| AMD-GPU | FP32 | 10M | C2 | 562.321 | 0.810 | 0.012 | 455.336 | 640.20 | no | YES |
| AMD-GPU | FP32 | 10M | C3 | 833.074 | 0.547 | 0.001 | 455.336 | 432.13 | no | YES |
| AMD-GPU | FP32 | 10M | C4 | 494.657 | 0.921 | 0.003 | 455.336 | 727.78 | no | YES |
| AMD-GPU | FP32 | 10M | baseline | 494.617 | 0.921 | 0.002 | 455.336 | 727.84 | no | YES |
| AMD-GPU | FP32 | 10000003 | C2 | 559.719 | 0.811 | 0.004 | 453.887 | 643.18 | no | YES |
| AMD-GPU | FP32 | 10000003 | C3 | 847.134 | 0.536 | 0.002 | 453.887 | 424.96 | no | YES |
| AMD-GPU | FP32 | 10000003 | C4 | 493.696 | 0.919 | 0.002 | 453.887 | 729.19 | no | YES |
| AMD-GPU | FP32 | 10000003 | baseline | 493.304 | 0.920 | 0.001 | 453.887 | 729.77 | no | YES |
| AMD-GPU | FP64 | 100K | C2 | 19.592 | 0.518 | 0.025 | 10.150 | 367.49 | no | YES |
| AMD-GPU | FP64 | 100K | C3 | 26.858 | 0.378 | 0.032 | 10.150 | 268.07 | no | YES |
| AMD-GPU | FP64 | 100K | C4 | 36.991 | 0.274 | 0.018 | 10.150 | 194.64 | no | YES |
| AMD-GPU | FP64 | 100K | baseline | 12.545 | 0.809 | 0.002 | 10.150 | 573.93 | no | YES |
| AMD-GPU | FP64 | 1M | C2 | 42.949 | 0.962 | 0.053 | 41.306 | 1676.41 | no | YES |
| AMD-GPU | FP64 | 1M | C3 | 72.909 | 0.567 | 0.030 | 41.306 | 987.54 | no | YES |
| AMD-GPU | FP64 | 1M | C4 | 99.521 | 0.415 | 0.106 | 41.306 | 723.46 | no | YES |
| AMD-GPU | FP64 | 1M | baseline | 40.921 | 1.009 | 0.026 | 41.306 | 1759.50 | no | no |
| AMD-GPU | FP64 | 10M | C2 | 987.139 | 0.937 | 0.009 | 925.307 | 729.38 | no | YES |
| AMD-GPU | FP64 | 10M | C3 | 1631.648 | 0.567 | 0.001 | 925.307 | 441.27 | no | YES |
| AMD-GPU | FP64 | 10M | C4 | 1051.162 | 0.880 | 0.004 | 925.307 | 684.96 | no | YES |
| AMD-GPU | FP64 | 10M | baseline | 957.475 | 0.966 | 0.000 | 925.307 | 751.98 | no | YES |
| AMD-GPU | FP64 | 10000003 | C2 | 984.061 | 0.939 | 0.001 | 924.401 | 731.66 | no | YES |
| AMD-GPU | FP64 | 10000003 | C3 | 1666.652 | 0.555 | 0.001 | 924.401 | 432.00 | no | YES |
| AMD-GPU | FP64 | 10000003 | C4 | 1039.945 | 0.889 | 0.001 | 924.401 | 692.34 | no | YES |
| AMD-GPU | FP64 | 10000003 | baseline | 957.659 | 0.965 | 0.001 | 924.401 | 751.83 | no | YES |
| CPU | FP32 | 100K | C2 | 23.909 | 0.948 | 0.738 | 22.676 | 150.57 | no | YES |
| CPU | FP32 | 100K | C3 | 66.487 | 0.341 | 0.143 | 22.676 | 54.15 | YES | YES |
| CPU | FP32 | 100K | C4 | 521.615 | 0.043 | 0.009 | 22.676 | 6.90 | YES | YES |
| CPU | FP32 | 100K | baseline | 30.813 | 0.736 | 0.527 | 22.676 | 116.83 | no | YES |
| CPU | FP32 | 1M | C2 | 64.360 | 1.239 | 0.223 | 79.714 | 559.36 | no | no |
| CPU | FP32 | 1M | C3 | 111.267 | 0.716 | 0.013 | 79.714 | 323.54 | no | YES |
| CPU | FP32 | 1M | C4 | 708.173 | 0.113 | 0.004 | 79.714 | 50.84 | no | YES |
| CPU | FP32 | 1M | baseline | 66.339 | 1.202 | 0.007 | 79.714 | 542.67 | no | no |
| CPU | FP32 | 10M | C2 | 4151.627 | 1.005 | 0.042 | 4172.681 | 86.71 | no | no |
| CPU | FP32 | 10M | C3 | 6255.203 | 0.667 | 0.021 | 4172.681 | 57.55 | no | YES |
| CPU | FP32 | 10M | C4 | 4523.933 | 0.922 | 0.044 | 4172.681 | 79.58 | no | YES |
| CPU | FP32 | 10M | baseline | 4156.109 | 1.004 | 0.024 | 4172.681 | 86.62 | no | no |
| CPU | FP32 | 10000003 | C2 | 4199.550 | 0.997 | 0.200 | 4185.260 | 85.72 | no | YES |
| CPU | FP32 | 10000003 | C3 | 6350.517 | 0.659 | 0.085 | 4185.260 | 56.69 | no | YES |
| CPU | FP32 | 10000003 | C4 | 4639.793 | 0.902 | 0.148 | 4185.260 | 77.59 | no | YES |
| CPU | FP32 | 10000003 | baseline | 4202.010 | 0.996 | 0.010 | 4185.260 | 85.67 | no | YES |
| CPU | FP64 | 100K | C2 | 70.757 | 0.305 | 0.111 | 21.610 | 101.76 | YES | YES |
| CPU | FP64 | 100K | C3 | 316.152 | 0.068 | 0.147 | 21.610 | 22.77 | YES | YES |
| CPU | FP64 | 100K | C4 | 207.693 | 0.104 | 0.181 | 21.610 | 34.67 | YES | YES |
| CPU | FP64 | 100K | baseline | 80.950 | 0.267 | 0.106 | 21.610 | 88.94 | YES | YES |
| CPU | FP64 | 1M | C2 | 88.463 | 0.929 | 0.140 | 82.197 | 813.90 | no | YES |
| CPU | FP64 | 1M | C3 | 188.957 | 0.435 | 0.180 | 82.197 | 381.04 | no | YES |
| CPU | FP64 | 1M | C4 | 246.472 | 0.333 | 0.774 | 82.197 | 292.12 | no | YES |
| CPU | FP64 | 1M | baseline | 84.367 | 0.974 | 2.104 | 82.197 | 853.41 | no | YES |
| CPU | FP64 | 10M | C2 | 8254.508 | 0.996 | 0.082 | 8220.940 | 87.23 | no | YES |
| CPU | FP64 | 10M | C3 | 13038.777 | 0.630 | 0.007 | 8220.940 | 55.22 | no | YES |
| CPU | FP64 | 10M | C4 | 8336.053 | 0.986 | 0.019 | 8220.940 | 86.37 | no | YES |
| CPU | FP64 | 10M | baseline | 8416.069 | 0.977 | 0.021 | 8220.940 | 85.55 | no | YES |
| CPU | FP64 | 10000003 | C2 | 8292.396 | 1.011 | 0.003 | 8384.381 | 86.83 | no | no |
| CPU | FP64 | 10000003 | C3 | 12974.714 | 0.646 | 0.082 | 8384.381 | 55.49 | no | YES |
| CPU | FP64 | 10000003 | C4 | 8424.140 | 0.995 | 0.100 | 8384.381 | 85.47 | no | YES |
| CPU | FP64 | 10000003 | baseline | 8262.009 | 1.015 | 0.005 | 8384.381 | 87.15 | no | no |
| CPU-gcc | FP32 | 100K | baseline | 6029.967 | 4.139 | 0.019 | 24956.410 | 0.60 | YES | no |
| CPU-gcc | FP32 | 1M | baseline | 89.782 | 299.283 | 36.369 | 26870.357 | 400.97 | no | no |
| CPU-gcc | FP32 | 10M | baseline | 9069.234 | 3.670 | 0.295 | 33279.883 | 39.69 | no | no |
| CPU-gcc | FP32 | 10000003 | baseline | 9021.101 | 3.635 | 0.215 | 32787.258 | 39.91 | no | no |
| CPU-gcc | FP64 | 100K | baseline | 6070.025 | 4.157 | 0.093 | 25234.135 | 1.19 | YES | no |
| CPU-gcc | FP64 | 1M | baseline | 121.548 | 240.841 | 21.737 | 29273.740 | 592.36 | no | no |
| CPU-gcc | FP64 | 10M | baseline | 12245.200 | 3.008 | 0.164 | 36836.582 | 58.80 | no | no |
| CPU-gcc | FP64 | 10000003 | baseline | 12209.770 | 3.292 | 0.008 | 40198.602 | 58.97 | no | no |
| Intel-GPU | FP32 | 100K | C2 | 113.651 | 0.241 | 0.035 | 27.433 | 31.68 | no | YES |
| Intel-GPU | FP32 | 100K | C3 | 69.764 | 0.393 | 0.048 | 27.433 | 51.60 | no | YES |
| Intel-GPU | FP32 | 100K | C4 | 374.306 | 0.073 | 0.070 | 27.433 | 9.62 | YES | YES |
| Intel-GPU | FP32 | 100K | baseline | 38.385 | 0.715 | 0.186 | 27.433 | 93.79 | no | YES |
| Intel-GPU | FP32 | 1M | C2 | 266.942 | 0.518 | 0.070 | 138.405 | 134.86 | no | YES |
| Intel-GPU | FP32 | 1M | C3 | 180.382 | 0.767 | 0.034 | 138.405 | 199.58 | no | YES |
| Intel-GPU | FP32 | 1M | C4 | 585.715 | 0.236 | 0.002 | 138.405 | 61.46 | no | YES |
| Intel-GPU | FP32 | 1M | baseline | 97.674 | 1.417 | 0.010 | 138.405 | 368.57 | no | no |
| Intel-GPU | FP32 | 10M | C2 | 1308.933 | 0.628 | 0.048 | 822.623 | 275.03 | no | YES |
| Intel-GPU | FP32 | 10M | C3 | 1507.504 | 0.546 | 0.003 | 822.623 | 238.81 | no | YES |
| Intel-GPU | FP32 | 10M | C4 | 6617.034 | 0.124 | 0.001 | 822.623 | 54.41 | no | YES |
| Intel-GPU | FP32 | 10M | baseline | 894.431 | 0.920 | 0.000 | 822.623 | 402.49 | no | YES |
| Intel-GPU | FP32 | 10000003 | C2 | 1131.808 | 0.723 | 0.009 | 817.948 | 318.08 | no | YES |
| Intel-GPU | FP32 | 10000003 | C3 | 1474.664 | 0.555 | 0.000 | 817.948 | 244.12 | no | YES |
| Intel-GPU | FP32 | 10000003 | C4 | 6607.750 | 0.124 | 0.000 | 817.948 | 54.48 | no | YES |
| Intel-GPU | FP32 | 10000003 | baseline | 893.377 | 0.916 | 0.001 | 817.948 | 402.97 | no | YES |

## PRIMARY ranking - geomean over ALL equally-weighted cases, primary archs (CPU-icpx + AMD-GPU + Intel-GPU); this is the selection metric

| rank | candidate | cases | geomean | worst | worst case | regressions |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | baseline | 20 | 0.8864 | 0.2670 | CPU FP64 100K | 14 |
| 2 | C2 | 20 | 0.7339 | 0.2414 | Intel-GPU FP32 100K | 17 |
| 3 | C3 | 20 | 0.4759 | 0.0684 | CPU FP64 100K | 20 |
| 4 | C4 | 20 | 0.3360 | 0.0435 | CPU FP32 100K | 20 |

## SECONDARY ranking - all archs including the CPU-gcc toolchain experiment

| rank | candidate | cases | geomean | worst | worst case | regressions |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | baseline | 28 | 1.8028 | 0.2670 | CPU FP64 100K | 14 |
| 2 | C2 | 20 | 0.7339 | 0.2414 | Intel-GPU FP32 100K | 17 |
| 3 | C3 | 20 | 0.4759 | 0.0684 | CPU FP64 100K | 20 |
| 4 | C4 | 20 | 0.3360 | 0.0435 | CPU FP32 100K | 20 |

## Achieved throughput and bandwidth ceilings

Traffic model: 36 B/row FP32 and 72 B/row FP64 (a 3-component row is read twice and written once: 3 floats in x2 = 24 B read + 12 B write = 36 B FP32; doubled for FP64). The SoA candidate C3 uses the `crossSoA` kernel, which reads the same logical 3-vectors from planar arrays and writes the same output, so the per-row byte count is unchanged; the same 36/72 B-per-row model is applied and this is an assumption, not a measurement. Bytes/row and the case GB/s are in the per-case table above.

Ceilings used for the bottleneck verdict: CPU ~170 GB/s L3 / ~64 GB/s DRAM (EPYC 7452), AMD gfx1100 ~960 GB/s HBM (RX 7900 XTX), Intel Arc A770 ~560 GB/s (FP32 only).

## Candidate vs candidate (geomean of median_A / median_B over shared cells)

Ratios >1 mean row candidate is slower than column candidate. Primary archs, overhead-dominated cells dropped.

| A \ B | baseline | C2 | C3 | C4 |
| --- | --- | --- | --- | --- |
| baseline | 1.000 | 0.808 | 0.569 | 0.456 |
| C2 | 1.238 | 1.000 | 0.724 | 0.546 |
| C3 | 1.757 | 1.381 | 1.000 | 0.799 |
| C4 | 2.194 | 1.830 | 1.251 | 1.000 |

