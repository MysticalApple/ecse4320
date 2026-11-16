#!/bin/bash
# Working-set sweep: MLC idle latency (16 KiB..2 GiB) + kern single-thread seq bandwidth (12 KiB..1.5 GiB total).
source "$(dirname "$0")/lib.sh"
init working_set rep,metric,size_kb,value
for r in $(seq $R); do
  for e in $(seq 4 21); do kb=$((1<<e))
    echo "$r,lat_ns,$kb,$(mlcr lat_${kb}_$r --idle_latency -r -c$CORE -b$kb | idle_ns)" >> $CSV
  done
  for e in $(seq 12 29); do
    kr mul $((1<<e)) seq 8 4k 0.5 | tee "$RAW/bw_${e}_$r.txt" | awk -F, -v r=$r '{print r",bw_gbps,"$1/1024","$3}' >> $CSV
  done
done
