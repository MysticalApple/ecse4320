# Sourced by every script. Override knobs via env, e.g. R=3 CORE=4 scripts/01_zero_queue.sh
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
R=${R:-5}                     # repetitions per point
CORE=${CORE:-2}               # core for single-thread runs
KERN=$ROOT/bin/kern
PIN="taskset -c $CORE"
# perf events: names differ across kernels/CPUs. Check `perf list | grep -iE 'tlb|l2_cache|dmnd'` and override.
EV_L1=${EV_L1:-L1-dcache-load-misses}
EV_LLC=${EV_LLC:-cache-misses}
EV_TLB=${EV_TLB:-dTLB-load-misses}
PERF_HDR=total_bytes,ns_per_elem,gbps,elems,ok,cycles,instr,l1_miss,llc_miss,tlb_miss,task_ms

# init <test> <csv header>: sets RAW, CSV and truncates the CSV
init() { T=$1; RAW=$ROOT/data/raw/$T; CSV=$ROOT/data/csv/$T.csv
         mkdir -p "$RAW" "$ROOT/data/csv" "$ROOT/plots"; echo "$2" > "$CSV"; }
# mlcr <tag> <mlc args>: run MLC (prefetchers untouched), keep raw output, echo it
mlcr() { local t=$1; shift; mlc -e "$@" | tee "$RAW/$t.txt"; }
idle_ns() { awk '/Each iteration/{print $(NF-1)}'; }                 # -> ns
loaded()  { awk 'NF==3 && $1~/^[0-9]+$/{print $1","$2","$3}'; }      # -> delay,lat_ns,bw_MBps
kr() { $PIN "$KERN" "$@"; }
# perfkern <tag> <kern args>: prints PERF_HDR row (kern fields + perf counters)
perfkern() {
  local t=$1 p="$RAW/$1.perf"; shift
  $PIN perf stat -x, -o "$p" -e task-clock,cycles,instructions,$EV_L1,$EV_LLC,$EV_TLB -- "$KERN" "$@" | tee "$RAW/$t.txt" | tr -d '\n'
  awk -F, -v a=$EV_L1 -v b=$EV_LLC -v c=$EV_TLB '/^#/||NF<3{next}{v[$3]=$1}
    END{printf ",%s,%s,%s,%s,%s,%s\n",v["cycles"],v["instructions"],v[a],v[b],v[c],v["task-clock"]}' "$p"
}
