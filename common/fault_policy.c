#include "fault_types.h"

static int is_abnormal(uint8_t t)
{
    return (t == FAULT_HARDFAULT) || (t == FAULT_MEMMANAGE) || (t == FAULT_BUSFAULT) || (t == FAULT_USAGEFAULT) ||
           (t == FAULT_WDG_RESET) || (t == FAULT_STACK_OVF) || (t == FAULT_MALLOC) || (t == FAULT_ASSERT);
}
static int is_boundary(uint8_t t) { return (t == FAULT_NONE) || (t == FAULT_COLD_BOOT) || (t == FAULT_BOOT_ROLLBACK); }

uint32_t fault_policy_trailing_abnormal(const uint8_t *types, uint32_t n)
{
    uint32_t count = 0u;
    for (uint32_t i = n; i > 0u; i--) {
        uint8_t t = types[i - 1u];
        if (is_boundary(t)) { break; }
        if (is_abnormal(t)) { count++; }          /* everything else (SOFT_RESET, TASK_WDG, ...) is neutral */
    }
    return count;
}
