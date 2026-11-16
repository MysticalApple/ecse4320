#!/bin/bash
# Cache-miss impact: footprint sweep (seq vs rand, 4K pages) with perf counters.
source "$(dirname "$0")/lib.sh"
init cache_miss "rep,pattern,array_bytes,$PERF_HDR"
for r in $(seq $R); do
  for p in seq rand; do
    for e in $(seq 12 28); do b=$((1<<e))
      echo "$r,$p,$b,$(perfkern ${p}_${e}_$r mul $b $p 8 4k 1)" >> $CSV
    done
  done
done
