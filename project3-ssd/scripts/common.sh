# Sourced by setup.sh, run.sh and env_info.sh (not run directly). Loads config, defines helpers.
source "$(dirname "${BASH_SOURCE[0]}")/../config.env"

# QUICK=1: smoke test with tiny runtimes, results go to results_quick/ (not report quality)
if [ "${QUICK:-0}" = 1 ]; then
  REPS=1 RUNTIME=2 RAMP=1 BASE_RUNTIME=3 TAIL_RUNTIME=3 TAIL_REPS=1 STATE_SECS=20 IDLE_SEC=2 COOL_TIMEOUT=0
  RESULTS=results_quick
fi
RESULTS=${RESULTS:-results}
export TESTFILE FILE_SIZE_G IOENGINE

die()  { echo "ERROR: $*" >&2; exit 1; }
warn() { echo "WARNING: $*" >&2; }

SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"
# Optional root-only step: never prompts, never aborts the run.
try_root() { { if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo -n "$@"; fi; } || warn "skipped/failed (needs root?): $*"; }

# NVMe composite temperature in whole degrees C (no root needed); prints nothing if unavailable.
nvme_temp() {
  local f
  for f in /sys/class/nvme/"$NVME_CTRL"/hwmon*/temp1_input; do
    [ -r "$f" ] && { echo $(( $(cat "$f") / 1000 )); return 0; }
  done
  return 0
}

# Block until the drive is below TEMP_MAX_C (or COOL_TIMEOUT seconds have passed).
wait_cool() {
  local t waited=0
  while t=$(nvme_temp); [ -n "$t" ] && [ "$t" -gt "$TEMP_MAX_C" ] && [ "$waited" -lt "$COOL_TIMEOUT" ]; do
    echo "cooling: ${t}C > ${TEMP_MAX_C}C"; sleep 5; waited=$((waited + 5))
  done
  if [ "$waited" -gt 0 ] && [ "$waited" -ge "$COOL_TIMEOUT" ]; then warn "still hot after ${waited}s"; fi
  return 0
}
