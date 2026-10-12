#!/usr/bin/env bash
# Cross-builds bootloader, application images (slot A/B + update/bad variants) and the CAN power node into ./build
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/keygen.py >/dev/null          # creates keys/ + bootloader/inc/pubkey.h only if missing
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake "$@"
cmake --build build
echo; arm-none-eabi-size build/bootloader.elf build/app_a.elf build/power_node.elf
ls -1 build/*.signed.bin
