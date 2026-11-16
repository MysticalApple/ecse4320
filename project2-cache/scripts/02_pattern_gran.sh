#!/bin/bash
# Sequential strides vs random: idle latency (1 GiB chain) and loaded read bandwidth at delay 0.
source "$(dirname "$0")/lib.sh"
init pattern_gran rep,pattern,stride_b,idle_ns,loaded_ns,bw_mbps
for r in $(seq $R); do
  for c in "seq 64" "seq 256" "seq 1024" "rand 64"; do set -- $c
    if [ $1 = rand ]; then li=-r; lo=-U; else li=; lo=; fi
    idle=$(mlcr idle_$1_$2_$r --idle_latency -c$CORE -b1048576 -l$2 $li | idle_ns)
    ld=$(mlcr load_$1_$2_$r --loaded_latency -d0 -R -l$2 $lo | loaded | cut -d, -f2,3)
    echo "$r,$1,$2,$idle,$ld" >> $CSV
  done
done
