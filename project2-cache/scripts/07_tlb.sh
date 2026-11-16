#!/bin/bash
# TLB impact: 4K vs 2M pages. (pat) fixed 256 MiB/array, varying access pattern; (fp) page-stride, varying footprint.
source "$(dirname "$0")/lib.sh"
init tlb "rep,sweep,pages,pattern,stride_b,array_bytes,$PERF_HDR"
A=$((1<<28))
for r in $(seq $R); do
  for pg in 4k 2m; do
    for c in "seq 8" "stride 64" "stride 512" "stride 4096" "stride 65536" "rand 8"; do set -- $c
      echo "$r,pat,$pg,$1,$2,$A,$(perfkern pat_${pg}_$1_$2_$r mul $A $1 $2 $pg 1)" >> $CSV
    done
    for e in $(seq 20 29); do
      echo "$r,fp,$pg,stride,4096,$((1<<e)),$(perfkern fp_${pg}_${e}_$r mul $((1<<e)) stride 4096 $pg 1)" >> $CSV
    done
  done
done
