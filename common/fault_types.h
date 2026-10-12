/* fault_types.h - fault record types (shared with host tools: keep in sync with tools/fc_link.py) */
#ifndef FAULT_TYPES_H
#define FAULT_TYPES_H
#include <stdint.h>
typedef enum {
    FAULT_NONE = 0, FAULT_HARDFAULT, FAULT_MEMMANAGE, FAULT_BUSFAULT, FAULT_USAGEFAULT, FAULT_WDG_RESET,
    FAULT_BROWNOUT, FAULT_TASK_WDG, FAULT_STACK_OVF, FAULT_MALLOC, FAULT_ASSERT, FAULT_NODE_LOST,
    FAULT_SENSOR, FAULT_COLD_BOOT, FAULT_SOFT_RESET, FAULT_BOOT_ROLLBACK
} fault_type_t;

/* Boot-time policy: number of consecutive abnormal resets (HardFault, watchdog, stack overflow ...)
 * at the tail of the log since the last boundary (healthy marker FAULT_NONE, power-on, or image rollback). */
uint32_t fault_policy_trailing_abnormal(const uint8_t *types_oldest_first, uint32_t n);
#endif
