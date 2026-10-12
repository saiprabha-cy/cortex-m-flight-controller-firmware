#!/usr/bin/env bash
# Static analysis: cppcheck (incl. MISRA-C addon) + clang-tidy on first-party code. Reports go to build/static/.
set -uo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/static
SRC="bsp bootloader/src app/src drivers common node"
DEFS="-DSTM32F407xx -D__GNUC__=13 -D__ARM_ARCH_7EM__=1 -D__ARM_FEATURE_DSP=1 -D__ARM_FP=4"
INC="-Ithird_party/cmsis_device_f4 -Ithird_party/cmsis_core -Ibsp -Idrivers -Icommon -Iconfig -Iapp/inc -Ibootloader/inc -Ithird_party/freertos/include -Ithird_party/freertos/portable/ARM_CM4F"
echo "== cppcheck (warnings, style, performance, portability)"
cppcheck --enable=warning,style,performance,portability --inline-suppr --std=c11 --error-exitcode=1 \
         --suppress=missingIncludeSystem --suppress='*:third_party/*' \
         --suppress=comparePointers:bsp/startup.c --suppress=comparePointers:bsp/bsp.c --suppress=nullPointer:app/src/comms.c \
         --suppress=knownConditionTrueFalse --suppress=constParameterPointer --suppress=constVariablePointer \
         --suppress=unreachableCode --suppress=duplicateCondition --suppress=unusedFunction $DEFS $INC $SRC 2> build/static/cppcheck.txt
rc=$?; cat build/static/cppcheck.txt | head -40
echo "== MISRA-C:2012 (cppcheck addon misra.py, rule numbers only: the licensed rule-text file cannot be redistributed)"
ADD=build/static/addons; mkdir -p $ADD
for f in misra.py cppcheckdata.py misra_9.py; do
  [ -f $ADD/$f ] || curl -sfL -o $ADD/$f https://raw.githubusercontent.com/danmar/cppcheck/2.13.0/addons/$f || echo "could not download $f (needs internet)"
done
rm -f build/static/misra.txt; touch build/static/misra.txt
for f in $(find $SRC -name '*.c' | sort); do
  cppcheck --dump --quiet $DEFS $INC "$f" 2>/dev/null
  python3 $ADD/misra.py "$f.dump" 2>&1 | grep -E "misra-c2012" >> build/static/misra.txt
  rm -f "$f.dump" "$f".*.dump
done
echo "MISRA findings: $(wc -l < build/static/misra.txt)  by rule:"; grep -oE "misra-c2012-[0-9.]+" build/static/misra.txt | sort | uniq -c | sort -rn | head -12
echo "== clang-tidy (bugprone + cert subset)"
for f in $(find $SRC -name '*.c' | sort); do
  clang-tidy "$f" -checks='-*,bugprone-*,cert-*,-bugprone-easily-swappable-parameters' -- -std=c11 $DEFS $INC --target=arm-none-eabi -mcpu=cortex-m4 -mthumb 2>/dev/null
done > build/static/clang-tidy.txt || true
echo "clang-tidy warnings: $(grep -c 'warning:' build/static/clang-tidy.txt || true)  (build/static/clang-tidy.txt)"
exit $rc
