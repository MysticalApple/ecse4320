#!/bin/bash
# Read/write mixes at max injection (delay 0). Write-only uses MLC's only write-only mode (-W6, non-temporal).
source "$(dirname "$0")/lib.sh"
init rw_mix rep,mix,lat_ns,bw_mbps
for r in $(seq $R); do
  for m in "100R -R" "2R:1W -W2" "3R:1W -W3" "1R:1W -W5" "100W-NT -W6"; do set -- $m
    echo "$r,$1,$(mlcr ${1}_$r --loaded_latency -d0 $2 | loaded | cut -d, -f2,3)" >> $CSV
  done
done
