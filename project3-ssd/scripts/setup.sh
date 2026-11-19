#!/usr/bin/env bash
# Safety checks, create + fully write the test file, drain the SLC cache.
# Usage: scripts/setup.sh        (FORCE=1 to reuse/overwrite an existing test file)
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/common.sh

dir=$(dirname "$TESTFILE")
src=$(findmnt -no SOURCE -T "$dir"); fstype=$(findmnt -no FSTYPE -T "$dir")
echo "test file: $TESTFILE on $src ($fstype), size ${FILE_SIZE_G} GiB"

[ "$src" = "$EXPECTED_DEV" ] || die "$dir is on $src, expected $EXPECTED_DEV (edit EXPECTED_DEV in config.env if this is right)"
case $fstype in
  tmpfs|ramfs|overlay|nfs*) die "$fstype is not a real SSD filesystem" ;;
  btrfs|zfs) warn "$fstype is copy-on-write and distorts overwrite behaviour; ext4/xfs recommended" ;;
esac

if [ -e "$TESTFILE" ]; then
  [ "${FORCE:-0}" = 1 ] || die "$TESTFILE exists; rerun with FORCE=1 to overwrite it"
else
  need=$(( FILE_SIZE_G * 1024 * 1024 * 1024 * 11 / 10 ))
  avail=$(df --output=avail -B1 "$dir" | tail -1)
  [ "$avail" -ge "$need" ] || die "not enough free space in $dir (need $need B, have $avail B)"
fi

# creating a file in / needs root; hand it to the current user so the benchmarks run unprivileged
[ "$(id -u)" -eq 0 ] || sudo -v || warn "no sudo: cannot create $TESTFILE in $dir"
$SUDO touch "$TESTFILE"
$SUDO chown "${SUDO_USER:-$USER}" "$TESTFILE"

# optional root tweaks (skipped with a warning if unavailable)
for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
  echo performance | $SUDO tee "$g" > /dev/null || { warn "could not set performance governor"; break; }
done

echo "preconditioning: writing the whole file sequentially"
fio fio/precondition.fio
try_root fstrim -v "$(findmnt -no TARGET -T "$dir")"
echo "idling ${SETTLE_SEC}s so the SLC cache drains"
sleep "$SETTLE_SEC"
echo "setup done. Next: QUICK=1 scripts/run.sh all   (smoke test), then scripts/run.sh all"
