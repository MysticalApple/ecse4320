#!/bin/bash
# Throughput-latency curve: MLC's default injection-delay sweep, read-only.
source "$(dirname "$0")/lib.sh"
init intensity rep,delay,lat_ns,bw_mbps
for r in $(seq $R); do
  mlcr sweep_$r --loaded_latency -R | loaded | sed "s/^/$r,/" >> $CSV
done
