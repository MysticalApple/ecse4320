#!/usr/bin/env bash
# Run the benchmarks. Usage:
#   scripts/run.sh [all|baseline|blocksize|rwmix|qdsweep|tail|range|pitfall|state]
#   QUICK=1 scripts/run.sh all     # smoke test, writes to results_quick/
# Finished runs are skipped, so an interrupted run can simply be restarted.
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/common.sh
RAW=$RESULTS/raw

# One fio run = REPS repetitions of fio/bench.fio with the given knobs.
#   job EXP REPS [KEY=VALUE ...]   keys: RW BS QD MIX RANGE_G DIRECT ENGINE RT TAG
# QD is the effective queue depth (numjobs x iodepth). Random IO above QD 32 uses
# several jobs of depth 32 because one thread can run out of CPU before the SSD saturates.
# Sequential IO always uses one job so jobs do not read the same offsets.
job() {
  local exp=$1 reps=$2; shift 2
  local RW=randread BS=4k QD=1 MIX=50 RANGE_G=$FILE_SIZE_G DIRECT=1 ENGINE=$IOENGINE RT=$RUNTIME TAG=none kv
  for kv in "$@"; do printf -v "${kv%%=*}" %s "${kv#*=}"; done
  local NUMJOBS=1 IODEPTH=$QD RAMP_T=$RAMP
  if [[ $RW == rand* && $QD -gt 32 ]]; then NUMJOBS=$((QD / 32)); IODEPTH=32; fi
  export RW BS NUMJOBS IODEPTH MIX RANGE_G DIRECT ENGINE RT RAMP_T

  local d=$RAW/$exp r base t
  mkdir -p "$d"
  for ((r = 1; r <= reps; r++)); do
    base="$d/rw=$RW,bs=$BS,qd=$QD,mix=$MIX,range=$RANGE_G,direct=$DIRECT,engine=$ENGINE,tag=$TAG,rep=$r"
    [ -s "$base.json" ] && continue
    echo "[$exp] rw=$RW bs=$BS qd=$QD (${NUMJOBS}x${IODEPTH}) mix=$MIX range=${RANGE_G}G $ENGINE direct=$DIRECT tag=$TAG rep=$r/$reps"
    t=$(nvme_temp)
    fio --output-format=json --output="$base.json.tmp" fio/bench.fio
    mv "$base.json.tmp" "$base.json"
    echo "$t $(nvme_temp)" > "$base.temp"
  done
}

# 1. Zero-queue baseline: QD=1 latency
exp_baseline() {
  wait_cool
  local rw bs
  for rw in randread randwrite read write; do
    bs=4k; [[ $rw == rand* ]] || bs=128k
    job baseline "$REPS" RW=$rw BS=$bs QD=1 RT=$BASE_RUNTIME
  done
}

# 2. Block-size sweep at QD=1 and QD=32, random and sequential, read and write
exp_blocksize() {
  wait_cool
  local rw qd bs sizes
  for rw in randread randwrite read write; do
    sizes="4k 16k 32k 64k 128k 256k"; [[ $rw == rand* ]] || sizes+=" 1m"
    for qd in 1 32; do
      for bs in $sizes; do job blocksize "$REPS" RW=$rw BS=$bs QD=$qd; done
    done
  done
}

# 3. Read/write mix (MIX = read percentage), 4K random, QD=32
exp_rwmix() {
  wait_cool
  local mix
  for mix in 100 70 50 0; do job rwmix "$REPS" RW=randrw MIX=$mix BS=4k QD=32; done
}

# 4. Queue-depth sweep (throughput vs latency, knee, Little's Law)
exp_qdsweep() {
  wait_cool
  local rw bs qd
  for rw in randread randwrite read; do
    bs=4k; [[ $rw == rand* ]] || bs=128k
    for qd in 1 2 4 8 16 32 64 128 256; do job qdsweep "$REPS" RW=$rw BS=$bs QD=$qd; done
  done
}

# 5. Tail latency at mid-QD and near-knee QD (longer runs for the high percentiles)
exp_tail() {
  wait_cool
  local rw qd
  for rw in randread randwrite; do
    for qd in $TAIL_QDS; do job tail "$TAIL_REPS" RW=$rw QD=$qd RT=$TAIL_RUNTIME; done
  done
}

# 6. LBA range / working set (GiB of the file that is touched)
exp_range() {
  wait_cool
  local rg rw
  for rg in 1 8 64 256; do
    for rw in randread randwrite; do job range "$REPS" RW=$rw QD=32 RANGE_G=$rg; done
  done
}

# 8. Misleading setups: 4K random read over a 4 GiB range
exp_pitfall() {
  wait_cool
  job pitfall "$REPS" RW=randread QD=32 RANGE_G=4 TAG=direct
  job pitfall "$REPS" RW=randread QD=32 RANGE_G=4 TAG=buffered DIRECT=0
  job pitfall "$REPS" RW=randread QD=32 RANGE_G=4 TAG=psync ENGINE=psync   # iodepth is ignored: effective QD=1
}

# 7. Device state: before / sustained sequential write / after. Runs last because it changes the drive.
exp_state() {
  local d=$RAW/state rw
  mkdir -p "$d"
  try_root fstrim -v "$(findmnt -no TARGET -T "$TESTFILE")"
  echo "[state] idling ${IDLE_SEC}s"; sleep "$IDLE_SEC"; wait_cool
  for rw in randread randwrite; do job state "$REPS" RW=$rw QD=32 TAG=before; done

  if [ ! -s "$d/steady.json" ]; then
    echo "[state] sustained 1M sequential write for ${STATE_SECS}s"
    ( t0=$(date +%s); while :; do echo "$(( $(date +%s) - t0 )) $(nvme_temp)"; sleep 1; done ) > "$d/steady_temp.log" &
    local tp=$!
    LOGPREFIX=$d/steady STATE_SECS=$STATE_SECS fio --output-format=json --output="$d/steady.json.tmp" fio/steady_state.fio
    kill "$tp" 2>/dev/null || true
    mv "$d/steady.json.tmp" "$d/steady.json"
  fi

  # no cooldown here on purpose: we want the drive in its post-burst state
  for rw in randread randwrite; do job state "$REPS" RW=$rw QD=32 TAG=after; done
}

preflight() {
  [ -w "$TESTFILE" ] || die "$TESTFILE missing or not writable; run scripts/setup.sh first"
  [ "$(stat -c %s "$TESTFILE")" -ge $((FILE_SIZE_G * 1024 * 1024 * 1024)) ] || die "$TESTFILE is smaller than ${FILE_SIZE_G} GiB"
  fio --version
  echo "NVMe temperature: $(nvme_temp) C, results -> $RESULTS"
}

what=${1:-all}
preflight
mkdir -p "$RESULTS"
# keep sudo alive (only needed for the optional fstrim) until this script exits
if [ "$(id -u)" -ne 0 ] && sudo -v 2>/dev/null; then
  ( while kill -0 $$ 2>/dev/null; do sudo -n true; sleep 50; done ) &
fi
scripts/env_info.sh > "$RESULTS/env.txt" 2>&1
if [ "$what" = all ]; then
  for e in baseline blocksize rwmix qdsweep tail range pitfall state; do "exp_$e"; done
else
  "exp_$what"
fi
echo "done in $((SECONDS / 60)) min. Next: scripts/plot.sh $RESULTS"
