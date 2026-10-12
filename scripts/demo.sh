#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"

mkdir -p "$ROOT/build/logs"

command -v renode >/dev/null || {
    echo "ERROR: Renode not found in PATH"
    exit 1
}

[ -f "$ROOT/build/bootloader.elf" ] || {
    echo "ERROR: build/bootloader.elf is missing"
    exit 1
}

echo "Project: $ROOT"
echo "Debug log: $ROOT/build/uart1_debug.log"

rm -f "$ROOT/build/uart1_debug.log"

exec renode --disable-xwt --console -e \
  "\$bootloader=@$ROOT/build/bootloader.elf; \$slotA=@$ROOT/build/app_a.signed.bin; \$uart1log=@$ROOT/build/uart1_debug.log; include @$ROOT/renode/flight_controller.resc; start"
