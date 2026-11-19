#!/usr/bin/env bash
# Usage: scripts/plot.sh [results_dir]     (default: results)
set -euo pipefail
cd "$(dirname "$0")/.."
r=${1:-results}
python3 analysis/parse.py "$r"
python3 analysis/plot.py "$r"
