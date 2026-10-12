# 06 MISRA-C:2012 and static-analysis report

**Status: NOT MISRA-compliant. This is a finding inventory with dispositions, not a compliance claim.**

## Method
* `./scripts/run_static.sh` runs cppcheck 2.13 (warning/style/performance/portability) and the cppcheck **MISRA addon** (`misra.py`) on every first-party `.c` file, then clang-tidy (bugprone, cert).
* The addon reports rule *numbers* only; the licensed rule texts are not redistributable. Vendor code (CMSIS, FreeRTOS, Monocypher, Unity) is excluded from the counts.
* Result of the last run: **811 unique first-party findings** (file:line:rule). Findings are neither suppressed nor hidden.

## Findings by rule (top 14) and disposition

| Rule | Count | Disposition |
|---|---|---|
| 11.4 | 191 | Casting between integer and pointer is unavoidable for memory-mapped I/O, flash addresses and the jump to the application (CMSIS register access). |
| 15.5 | 128 | Early returns / multiple exits are used for error handling (single-exit rule is advisory; kept for readability). |
| 12.3 | 114 | Comma operator is not used; findings are multiple declarators/initialisers (`uint32_t a = 0u, b = 1u;`). Mechanical clean-up candidate. |
| 17.7 | 37 | Return values of logging / best-effort calls are intentionally discarded with `(void)` in most places; remaining ones are listed for review. |
| 12.1 | 31 | Operator precedence made explicit in most places; remaining findings reviewed case by case. |
| 8.4 | 31 | External functions defined in one .c file and declared in another without a shared header (app-internal hooks such as `stats_fill`). Candidate for a private header. |
| 17.8 | 28 | Function parameters modified (e.g. pointer walking in startup/uart loops). |
| 10.4 | 27 | Mixed signedness in comparisons with literals; mostly suffixed with `u` already, remainder open. |
| 10.8 | 23 | Composite expression casts (register arithmetic). |
| 12.2 | 20 | Shift amounts derived from variables (GPIO pin masks). |
| 21.15 | 19 | `memcpy` between differently typed pointers for packed wire structs - deliberate. |
| 10.1 | 14 | Not yet reviewed. |
| 13.3 | 14 | Increment/decrement combined with other side effects in ring-buffer code. |
| 17.1 | 13 | Not yet reviewed. |

## cppcheck and clang-tidy
* cppcheck: no `error`/`warning` findings in first-party code after suppressing 3 documented false positives (comparison of linker-symbol addresses in `bsp/startup.c`, `bsp/bsp.c`; the *intentional* null call in the fault-injection command). Style findings about `const` pointer parameters are accepted (hardware register pointers).
* clang-tidy: 8 warnings, all `bugprone-reserved-identifier` for linker-script symbols (`_estack`, `_sidata`, ...) which must have those names.

## Next steps to approach compliance
1. Mechanical clean-ups: 12.3 (split declarators), 8.4 (private header for hooks), 10.4/10.8 (explicit casts).
2. Write a project deviation record for 11.4 and 21.15 (hardware access).
3. Obtain the MISRA rule texts and re-run to get rule descriptions; add a baseline file so CI fails on *new* findings only.
