#!/bin/bash
# Pitfalls: (a) "DRAM bandwidth" measured on cache-resident buffers; (b) unpinned / powersave governor on a short kernel.
source "$(dirname "$0")/lib.sh"
init pitfall rep,pitfall,variant,gbps,ns_per_elem,ghz
trap 'cpupower frequency-set -g performance >/dev/null' EXIT
row() { awk -F, -v r=$r -v v=$1 '{printf "%s,pin_governor,%s,%s,%s,%.2f\n",r,v,$3,$2,$6/($11*1e6)}'; }
for r in $(seq $R); do
  for v in "L2-resident 512" "L3-resident 3072" "DRAM 100000"; do set -- $v   # MLC -b = KiB per thread
    bw=$(mlcr cache_$1_$r --loaded_latency -d0 -R -b$2 | loaded | cut -d, -f3)
    echo "$r,cache_reuse,$1,$(awk "BEGIN{print $bw/1000}"),," >> $CSV
  done
  perfkern pinned_$r mul 65536 seq 8 4k 1 | row pinned_performance >> $CSV
  PIN= perfkern unpinned_$r mul 65536 seq 8 4k 1 | row unpinned_performance >> $CSV
  cpupower frequency-set -g powersave >/dev/null
  perfkern powersave_$r mul 65536 seq 8 4k 1 | row pinned_powersave >> $CSV
  cpupower frequency-set -g performance >/dev/null
done
