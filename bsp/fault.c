#include "fault.h"
#include "bsp.h"
#include "dbg.h"
#include "stm32f4xx.h"
#include "../config/memory_map.h"
#include <string.h>

static flashlog_t s_log; static bool s_ready; static uint8_t s_mode; static const flash_ops_t *s_fl;

const char *fault_type_name(uint8_t t)
{
    static const char *const n[] = { "NONE","HARDFAULT","MEMMANAGE","BUSFAULT","USAGEFAULT","WDG_RESET","BROWNOUT","TASK_WDG",
        "STACK_OVF","MALLOC_FAIL","ASSERT","NODE_LOST","SENSOR","COLD_BOOT","SOFT_RESET","BOOT_ROLLBACK" };
    return (t < (sizeof n / sizeof n[0])) ? n[t] : "?";
}

void fault_set_mode(uint8_t m) { s_mode = m; }

int fault_log_init(const flash_ops_t *fl)
{
    s_fl = fl;
    int rc = flashlog_init(&s_log, fl, FAULTLOG_BASE, FAULTLOG_SIZE, 0u, 0u);
    s_ready = (rc == FLASH_OK);
    return rc;
}

int fault_log_record(fault_type_t t, uint16_t info, uint32_t pc, uint32_t lr, uint32_t cfsr, uint32_t addr)
{
    if (!s_ready) { return FLASH_ERR; }
    fault_record_t r = { (uint8_t)t, s_mode, info, pc, lr, cfsr, addr, time_ms() };
    uint8_t p[FLOG_PAYLOAD_SIZE]; memcpy(p, &r, sizeof r);
    return flashlog_append(&s_log, p);
}
uint32_t fault_log_count(void) { return s_ready ? flashlog_count(&s_log) : 0u; }
int fault_log_read(uint32_t idx, fault_record_t *r)
{
    uint8_t p[FLOG_PAYLOAD_SIZE];
    if (!s_ready || !flashlog_read(&s_log, idx, p, NULL)) { return FLASH_ERR; }
    memcpy(r, p, sizeof *r); return FLASH_OK;
}
int fault_log_clear(void)
{
    if (!s_ready) { return FLASH_ERR; }
    if (s_fl->erase_sector(FAULTLOG_BASE) != FLASH_OK) { return FLASH_ERR; }
    return flashlog_init(&s_log, s_fl, FAULTLOG_BASE, FAULTLOG_SIZE, 0u, 0u);
}

/* Called with the stacked exception frame {r0,r1,r2,r3,r12,lr,pc,psr}. Never returns: log, print, reset. */
void fault_c_handler(uint32_t *sp, uint32_t exc_lr, fault_type_t type)
{
    __disable_irq();
    uint32_t pc = sp[6], lr = sp[5], psr = sp[7];
    uint32_t cfsr = SCB->CFSR, hfsr = SCB->HFSR;
    uint32_t addr = ((cfsr & 0x80u) != 0u) ? SCB->MMFAR : (((cfsr & 0x8000u) != 0u) ? SCB->BFAR : 0u);
    dbg_polled_printf(USART1, "\r\n*** %s *** pc=%08x lr=%08x psr=%08x\r\n    cfsr=%08x hfsr=%08x addr=%08x exc_lr=%08x r0=%08x r1=%08x r2=%08x r3=%08x\r\n",
                      fault_type_name((uint8_t)type), pc, lr, psr, cfsr, hfsr, addr, exc_lr, sp[0], sp[1], sp[2], sp[3]);
    (void)fault_log_record(type, 0u, pc, lr, cfsr, addr);
    dbg_polled_printf(USART1, "    fault logged, resetting\r\n");
    bsp_system_reset();
}

/* Naked trampolines choose MSP or PSP from EXC_RETURN bit 2 and pass the frame pointer to C. */
#define FAULT_TRAMPOLINE(name, type) \
    __attribute__((naked)) void name(void) { \
        __asm volatile ("tst lr, #4\n ite eq\n mrseq r0, msp\n mrsne r0, psp\n mov r1, lr\n movs r2, %0\n b fault_c_handler" :: "i"(type)); }

FAULT_TRAMPOLINE(HardFault_Handler,  FAULT_HARDFAULT)
FAULT_TRAMPOLINE(MemManage_Handler,  FAULT_MEMMANAGE)
FAULT_TRAMPOLINE(BusFault_Handler,   FAULT_BUSFAULT)
FAULT_TRAMPOLINE(UsageFault_Handler, FAULT_USAGEFAULT)

void bsp_enable_fault_handlers(void)
{
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk;
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk | SCB_CCR_UNALIGN_TRP_Msk | SCB_CCR_STKALIGN_Msk;
}
