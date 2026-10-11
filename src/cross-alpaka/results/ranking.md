# Campaign2 candidate ranking

Selection metric: geometric mean of speedup (ORNL reference / candidate) over ALL equally-weighted (architecture, size, dtype) cases, primary architectures only. Speedup < 1.0 is slower than the reference.

## PRIMARY ranking (selection metric)

| rank | candidate | cases | geomean | worst | worst case | regressions |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | baseline | 20 | 0.8864 | 0.2670 | CPU FP64 100K | 14 |
| 2 | C2 | 20 | 0.7339 | 0.2414 | Intel-GPU FP32 100K | 17 |
| 3 | C3 | 20 | 0.4759 | 0.0684 | CPU FP64 100K | 20 |
| 4 | C4 | 20 | 0.3360 | 0.0435 | CPU FP32 100K | 20 |

## SECONDARY ranking (all archs, incl. CPU-gcc)

| rank | candidate | cases | geomean | worst | worst case | regressions |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | baseline | 28 | 1.8028 | 0.2670 | CPU FP64 100K | 14 |
| 2 | C2 | 20 | 0.7339 | 0.2414 | Intel-GPU FP32 100K | 17 |
| 3 | C3 | 20 | 0.4759 | 0.0684 | CPU FP64 100K | 20 |
| 4 | C4 | 20 | 0.3360 | 0.0435 | CPU FP32 100K | 20 |

## Regressions - PRIMARY case set (speedup < 1.0)

### baseline
- AMD-GPU FP32 100K speedup 0.668 (median 10.798 us vs ORNL 7.209 us)
- AMD-GPU FP32 10M speedup 0.921 (median 494.617 us vs ORNL 455.336 us)
- AMD-GPU FP32 10000003 speedup 0.920 (median 493.304 us vs ORNL 453.887 us)
- AMD-GPU FP64 100K speedup 0.809 (median 12.545 us vs ORNL 10.150 us)
- AMD-GPU FP64 10M speedup 0.966 (median 957.475 us vs ORNL 925.307 us)
- AMD-GPU FP64 10000003 speedup 0.965 (median 957.659 us vs ORNL 924.401 us)
- CPU FP32 100K speedup 0.736 (median 30.813 us vs ORNL 22.676 us)
- CPU FP32 10000003 speedup 0.996 (median 4202.010 us vs ORNL 4185.260 us)
- CPU FP64 100K speedup 0.267 (median 80.950 us vs ORNL 21.610 us)
- CPU FP64 1M speedup 0.974 (median 84.367 us vs ORNL 82.197 us)
- CPU FP64 10M speedup 0.977 (median 8416.069 us vs ORNL 8220.940 us)
- Intel-GPU FP32 100K speedup 0.715 (median 38.385 us vs ORNL 27.433 us)
- Intel-GPU FP32 10M speedup 0.920 (median 894.431 us vs ORNL 822.623 us)
- Intel-GPU FP32 10000003 speedup 0.916 (median 893.377 us vs ORNL 817.948 us)
### C2
- AMD-GPU FP32 100K speedup 0.623 (median 11.578 us vs ORNL 7.209 us)
- AMD-GPU FP32 1M speedup 0.599 (median 34.470 us vs ORNL 20.652 us)
- AMD-GPU FP32 10M speedup 0.810 (median 562.321 us vs ORNL 455.336 us)
- AMD-GPU FP32 10000003 speedup 0.811 (median 559.719 us vs ORNL 453.887 us)
- AMD-GPU FP64 100K speedup 0.518 (median 19.592 us vs ORNL 10.150 us)
- AMD-GPU FP64 1M speedup 0.962 (median 42.949 us vs ORNL 41.306 us)
- AMD-GPU FP64 10M speedup 0.937 (median 987.139 us vs ORNL 925.307 us)
- AMD-GPU FP64 10000003 speedup 0.939 (median 984.061 us vs ORNL 924.401 us)
- CPU FP32 100K speedup 0.948 (median 23.909 us vs ORNL 22.676 us)
- CPU FP32 10000003 speedup 0.997 (median 4199.550 us vs ORNL 4185.260 us)
- CPU FP64 100K speedup 0.305 (median 70.757 us vs ORNL 21.610 us)
- CPU FP64 1M speedup 0.929 (median 88.463 us vs ORNL 82.197 us)
- CPU FP64 10M speedup 0.996 (median 8254.508 us vs ORNL 8220.940 us)
- Intel-GPU FP32 100K speedup 0.241 (median 113.651 us vs ORNL 27.433 us)
- Intel-GPU FP32 1M speedup 0.518 (median 266.942 us vs ORNL 138.405 us)
- Intel-GPU FP32 10M speedup 0.628 (median 1308.933 us vs ORNL 822.623 us)
- Intel-GPU FP32 10000003 speedup 0.723 (median 1131.808 us vs ORNL 817.948 us)
### C3
- AMD-GPU FP32 100K speedup 0.324 (median 22.258 us vs ORNL 7.209 us)
- AMD-GPU FP32 1M speedup 0.467 (median 44.230 us vs ORNL 20.652 us)
- AMD-GPU FP32 10M speedup 0.547 (median 833.074 us vs ORNL 455.336 us)
- AMD-GPU FP32 10000003 speedup 0.536 (median 847.134 us vs ORNL 453.887 us)
- AMD-GPU FP64 100K speedup 0.378 (median 26.858 us vs ORNL 10.150 us)
- AMD-GPU FP64 1M speedup 0.567 (median 72.909 us vs ORNL 41.306 us)
- AMD-GPU FP64 10M speedup 0.567 (median 1631.648 us vs ORNL 925.307 us)
- AMD-GPU FP64 10000003 speedup 0.555 (median 1666.652 us vs ORNL 924.401 us)
- CPU FP32 100K speedup 0.341 (median 66.487 us vs ORNL 22.676 us)
- CPU FP32 1M speedup 0.716 (median 111.267 us vs ORNL 79.714 us)
- CPU FP32 10M speedup 0.667 (median 6255.203 us vs ORNL 4172.681 us)
- CPU FP32 10000003 speedup 0.659 (median 6350.517 us vs ORNL 4185.260 us)
- CPU FP64 100K speedup 0.068 (median 316.152 us vs ORNL 21.610 us)
- CPU FP64 1M speedup 0.435 (median 188.957 us vs ORNL 82.197 us)
- CPU FP64 10M speedup 0.630 (median 13038.777 us vs ORNL 8220.940 us)
- CPU FP64 10000003 speedup 0.646 (median 12974.714 us vs ORNL 8384.381 us)
- Intel-GPU FP32 100K speedup 0.393 (median 69.764 us vs ORNL 27.433 us)
- Intel-GPU FP32 1M speedup 0.767 (median 180.382 us vs ORNL 138.405 us)
- Intel-GPU FP32 10M speedup 0.546 (median 1507.504 us vs ORNL 822.623 us)
- Intel-GPU FP32 10000003 speedup 0.555 (median 1474.664 us vs ORNL 817.948 us)
### C4
- AMD-GPU FP32 100K speedup 0.306 (median 23.596 us vs ORNL 7.209 us)
- AMD-GPU FP32 1M speedup 0.395 (median 52.345 us vs ORNL 20.652 us)
- AMD-GPU FP32 10M speedup 0.921 (median 494.657 us vs ORNL 455.336 us)
- AMD-GPU FP32 10000003 speedup 0.919 (median 493.696 us vs ORNL 453.887 us)
- AMD-GPU FP64 100K speedup 0.274 (median 36.991 us vs ORNL 10.150 us)
- AMD-GPU FP64 1M speedup 0.415 (median 99.521 us vs ORNL 41.306 us)
- AMD-GPU FP64 10M speedup 0.880 (median 1051.162 us vs ORNL 925.307 us)
- AMD-GPU FP64 10000003 speedup 0.889 (median 1039.945 us vs ORNL 924.401 us)
- CPU FP32 100K speedup 0.043 (median 521.615 us vs ORNL 22.676 us)
- CPU FP32 1M speedup 0.113 (median 708.173 us vs ORNL 79.714 us)
- CPU FP32 10M speedup 0.922 (median 4523.933 us vs ORNL 4172.681 us)
- CPU FP32 10000003 speedup 0.902 (median 4639.793 us vs ORNL 4185.260 us)
- CPU FP64 100K speedup 0.104 (median 207.693 us vs ORNL 21.610 us)
- CPU FP64 1M speedup 0.333 (median 246.472 us vs ORNL 82.197 us)
- CPU FP64 10M speedup 0.986 (median 8336.053 us vs ORNL 8220.940 us)
- CPU FP64 10000003 speedup 0.995 (median 8424.140 us vs ORNL 8384.381 us)
- Intel-GPU FP32 100K speedup 0.073 (median 374.306 us vs ORNL 27.433 us)
- Intel-GPU FP32 1M speedup 0.236 (median 585.715 us vs ORNL 138.405 us)
- Intel-GPU FP32 10M speedup 0.124 (median 6617.034 us vs ORNL 822.623 us)
- Intel-GPU FP32 10000003 speedup 0.124 (median 6607.750 us vs ORNL 817.948 us)

## Selected candidate

`baseline` is the PRIMARY geomean winner and is the candidate the PR selects.

