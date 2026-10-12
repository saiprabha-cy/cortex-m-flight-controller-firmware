/* fault.h - fault records in flash + Cortex-M fault exception capture. */
#ifndef FAULT_H
#define FAULT_H
#include <stdint.h>
#include "../common/flash_if.h"
#include "../common/flashlog.h"

#include "../common/fault_types.h"

/* 24 bytes = one flashlog payload */
typedef struct __attribute__((packed)) {
    uint8_t  type;       /* fault_type_t */
    uint8_t  mode;       /* sys_mode_t at the time */
    uint16_t info;       /* type specific (task id, reset count, ...) */
    uint32_t pc, lr, cfsr, addr, uptime_ms;
} fault_record_t;
_Static_assert(sizeof(fault_record_t) == FLOG_PAYLOAD_SIZE, "fault_record_t must be 24 bytes");

const char *fault_type_name(uint8_t t);
int      fault_log_init(const flash_ops_t *fl);
int      fault_log_record(fault_type_t t, uint16_t info, uint32_t pc, uint32_t lr, uint32_t cfsr, uint32_t addr);
uint32_t fault_log_count(void);
int      fault_log_read(uint32_t idx, fault_record_t *r);
int      fault_log_clear(void);
void     fault_set_mode(uint8_t mode);
void     bsp_enable_fault_handlers(void);
void     fault_c_handler(uint32_t *stacked, uint32_t exc_lr, fault_type_t type) __attribute__((noreturn));
#endif
