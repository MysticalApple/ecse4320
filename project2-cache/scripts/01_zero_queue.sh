#!/bin/bash
# Zero-queue latency per level: single latency thread, random chase, buffer sized per level.
source "$(dirname "$0")/lib.sh"
init zero_queue rep,level,buf_kb,ns
for r in $(seq $R); do
  for lv in "L1 16" "L2 512" "L3 8192" "DRAM 1048576"; do set -- $lv
    echo "$r,$1,$2,$(mlcr ${1}_$r --idle_latency -r -c$CORE -b$2 | idle_ns)" >> $CSV
  done
done
