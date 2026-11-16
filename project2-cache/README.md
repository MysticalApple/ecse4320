# ECSE 4320/6320 Project 2: Cache & Memory Profiling (Ryzen 7 9700X, Arch Linux)

## Run
Requires: `mlc` in PATH, `perf`, `cpupower`, `gcc`, `dmidecode`, python3 with pandas + matplotlib.
```
sudo scripts/run_all.sh          # everything: setup, 01-08, plots   (R=2 sudo -E ... for a quick pass)
sudo scripts/00_env.sh           # or step by step; then e.g. sudo scripts/04_intensity.sh && python3 plot.py
```
`00_env.sh` **disables SMT**, sets the `performance` governor, reserves 2048 x 2 MiB hugepages, builds `bin/kern`,
and writes `data/env.txt`. Undo SMT afterwards: `echo on | sudo tee /sys/devices/system/cpu/smt/control`.
Knobs (env vars): `R` repetitions (5), `CORE` pinned core for single-thread runs (2), `EV_L1/EV_LLC/EV_TLB` perf event names.

## Layout
`src/kern.c` the only custom C (z=x*y / saxpy; seq/stride/rand; 4K or 2M pages; self-verifying) | `scripts/` glue (one per experiment) |
`data/raw/<test>/` unmodified MLC/perf output | `data/csv/<test>.csv` one row per trial | `plot.py` -> `plots/<test>.pdf` | `docs/`.

| Script | Experiment | Tool |
|---|---|---|
| 01 | zero-queue latency L1/L2/L3/DRAM | `mlc --idle_latency -r` |
| 02 | seq strides 64/256/1024 B vs random | `mlc --idle_latency`, `--loaded_latency -l/-U` |
| 03 | R/W mixes (R, 2:1, 3:1, 1:1, write-only NT) | `mlc --loaded_latency -W` |
| 04 | throughput-latency + knee + Little's Law | `mlc --loaded_latency` sweep |
| 05 | working-set sweep | MLC latency + `kern` 1-thread BW |
| 06 | cache-miss vs perf counters | `kern` under `perf stat` |
| 07 | TLB: 4K vs 2M pages | `kern` under `perf stat` |
| 08 | pitfalls (cache reuse; unpinned / powersave) | MLC, `kern` |

## Things to check before trusting results
- **perf event names** on Zen 5 vary by kernel. `00_env.sh` warns on `<not supported>`. Defaults are generic aliases
  (`L1-dcache-load-misses`, `cache-misses`, `dTLB-load-misses`); find native ones with `perf list` and override. State which events you used in the report.
- Counters in 06/07 include `kern`'s init pass (page faults, first touch); 1 s runs keep it small but nonzero at large footprints.
- `mlc -e` leaves prefetchers on, so sequential idle-latency chases are partly hidden by prefetch; random (`-r`) is used where "true" latency is wanted.
- MLC uses hugepages, so its results have few TLB misses; the TLB study uses `kern` only.
- `kern` GB/s counts 24 B/element (2 reads + 1 write, no RFO). Write-only is non-temporal (`-W6`); MLC has no regular-store write-only mode. 2:1 and 3:1 approximate 70/30.
- Zen boost can't be locked: frequency is measured (`data/freq_ghz.txt`) and used for ns->cycles conversion.
