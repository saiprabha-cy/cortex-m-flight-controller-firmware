#!/usr/bin/env bash
# Host-side unit tests (native gcc, ASan+UBSan). Add COVERAGE=1 for gcov data.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S tests/unit -B build-host -G Ninja -DCOVERAGE=${COVERAGE:-OFF}
cmake --build build-host
ctest --test-dir build-host --output-on-failure
