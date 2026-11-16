#!/bin/bash
# Machine setup + environment snapshot + build. Needs root.
source "$(dirname "$0")/lib.sh"
[ $EUID = 0 ] || { echo "run as root" >&2; exit 1; }
echo off > /sys/devices/system/cpu/smt/control        # restore later: echo on > same file
cpupower frequency-set -g performance >/dev/null
modprobe msr || true
sysctl -qw vm.nr_hugepages=2048                        # 4 GB pool for MLC and kern 2m pages
[ "$(awk '/HugePages_Total/{print $2}' /proc/meminfo)" -ge 2048 ] || echo "WARN: hugepage pool < 2048 (fragmented? retry after reboot)" >&2
mkdir -p "$ROOT/bin" "$ROOT/data"
gcc -O2 -march=native -o "$KERN" "$ROOT/src/kern.c"

if perf stat -e $EV_L1,$EV_LLC,$EV_TLB true 2>&1 | grep -qi 'not supported'; then
  echo "WARN: some perf events unsupported; set EV_L1/EV_LLC/EV_TLB (see perf list)" >&2; fi

# observed core clock (boost can't be locked): cycles / task-clock during a short cache-resident run
RAW=$ROOT/data/raw/env; mkdir -p "$RAW"
perfkern freq mul 16384 seq 8 4k 1 | awk -F, '{print $6/($11*1e6)}' > "$ROOT/data/freq_ghz.txt"

set +e
{ date; lscpu; uname -a; gcc --version | head -1; perf --version; mlc --help 2>&1 | head -1
  echo "governor=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor) driver=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver)"
  echo "boost=$(cat /sys/devices/system/cpu/cpufreq/boost)  smt=$(cat /sys/devices/system/cpu/smt/control)"
  echo "thp=$(cat /sys/kernel/mm/transparent_hugepage/enabled)"; grep -i huge /proc/meminfo
  echo "pinned core=$CORE  observed freq GHz=$(cat "$ROOT/data/freq_ghz.txt")  events: $EV_L1 $EV_LLC $EV_TLB"
  dmidecode -t memory | grep -E 'Size:|Speed:|Locator:'; free -h
} > "$ROOT/data/env.txt" 2>&1
echo "wrote data/env.txt"
