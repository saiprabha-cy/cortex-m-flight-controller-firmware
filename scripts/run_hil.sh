#!/usr/bin/env bash
# Renode hardware-in-the-loop suite. Needs: renode on PATH (or RENODE=...), firmware built (./scripts/build.sh).
#   ./scripts/run_hil.sh                 everything (~25-40 min)
#   ./scripts/run_hil.sh -m "not slow"   skip firmware-upload scenarios
#   ./scripts/run_hil.sh -k fault        only the fault-injection campaign
set -euo pipefail
cd "$(dirname "$0")/.."
python3 -m pytest tests/integration -v "$@"
