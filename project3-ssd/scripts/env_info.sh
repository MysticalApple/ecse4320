#!/usr/bin/env bash
# Prints hardware/software/device state for the methodology section.
# Usage: scripts/env_info.sh > results/env.txt
cd "$(dirname "$0")/.."
source scripts/common.sh
hdr() { printf '\n### %s\n' "$*"; }
dir=$(dirname "$TESTFILE")
src=$(findmnt -no SOURCE -T "$dir" 2>/dev/null)
blk=$(lsblk -no PKNAME "$src" 2>/dev/null | head -1)

hdr date;      date -Is
hdr kernel;    uname -srvm
hdr os;        (. /etc/os-release && echo "$PRETTY_NAME")
hdr cpu;       lscpu | grep -E 'Model name|^CPU\(s\)|Thread|Core|Socket|max MHz'
hdr governor;  sort /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2>/dev/null | uniq -c
hdr "smt active (1=on)"; cat /sys/devices/system/cpu/smt/active 2>/dev/null
hdr "turbo";   cat /sys/devices/system/cpu/intel_pstate/no_turbo /sys/devices/system/cpu/cpufreq/boost 2>/dev/null
hdr memory;    free -h
hdr tools;     fio --version; python3 --version
python3 -c 'import matplotlib, numpy; print("matplotlib", matplotlib.__version__, "numpy", numpy.__version__)'
hdr "test file / filesystem"; ls -l "$TESTFILE"; findmnt -T "$dir"; df -h "$dir"
hdr "nvme device"
for f in model firmware_rev transport; do echo "$f: $(cat /sys/class/nvme/$NVME_CTRL/$f 2>/dev/null)"; done
for f in current_link_speed current_link_width max_link_speed max_link_width; do
  echo "$f: $(cat /sys/class/nvme/$NVME_CTRL/device/$f 2>/dev/null)"; done
echo "io scheduler: $(cat /sys/block/$blk/queue/scheduler 2>/dev/null)"
echo "nr_requests:  $(cat /sys/block/$blk/queue/nr_requests 2>/dev/null)"
echo "temperature:  $(nvme_temp) C"
hdr "smart-log (root only)"; try_root nvme smart-log "/dev/$NVME_CTRL" 2>&1
