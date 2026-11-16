#!/bin/bash
# sudo scripts/run_all.sh   (use R=2 for a quick pass)
set -e; cd "$(dirname "$0")"
for s in [0-9]*.sh; do echo "== $s"; bash "$s"; done
python3 ../plot.py
