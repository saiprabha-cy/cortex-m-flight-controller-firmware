/* bsp.h - board support: clocks, timebase, reset cause, stack monitor, cycle counter. */
#ifndef BSP_H
#define BSP_H
#include <stdint.h>
#include <stdbool.h>

#define HSE_HZ  8000000u            /* STM32F4-Discovery crystal */

extern uint32_t g_sysclk_hz, g_pclk1_hz, g_pclk2_hz;      /* set by bsp_clock_init() */

typedef enum { CLK_PLL_168MHZ = 0, CLK_HSI_FALLBACK = 1 } clk_status_t;
clk_status_t bsp_clock_init(void);          /* HSE+PLL -> 168 MHz, with bounded waits; falls back to 16 MHz HSI */

/* free-running 1 MHz timebase on TIM2 (32 bit) - independent of the RTOS tick */
void     bsp_timebase_init(void);
uint32_t time_us(void);
uint32_t time_ms(void);
void     delay_us(uint32_t us);
void     delay_ms(uint32_t ms);

/* DWT cycle counter for latency/jitter measurement */
void     dwt_init(void);
extern bool g_dwt_hw;                       /* false in emulators without a DWT unit: cycles are then derived from TIM2 (1 us resolution) */
uint32_t dwt_cycles(void);
uint32_t dwt_cycles_to_ns(uint32_t cycles);

/* reset cause (RCC->CSR) */
typedef enum { RST_UNKNOWN = 0, RST_POWER_ON, RST_BROWN_OUT, RST_PIN, RST_SOFTWARE, RST_IWDG, RST_WWDG, RST_LOW_POWER } reset_cause_t;
/* Reset cause. The first stage that runs (bootloader) reads the RCC flags (and clears them), then latches the result in
 * RAM so the application still sees it. With FC_EMU_RESET_INFERENCE (default in HIL builds) a missing 'clean reset'
 * marker is interpreted as a watchdog/lock-up reset - needed in emulators whose RCC model does not set IWDGRSTF. */
reset_cause_t bsp_reset_cause(void);
void          bsp_reset_cause_latch(reset_cause_t c);   /* bootloader: hand the cause to the application */
void          bsp_mark_running(void);                   /* application: from now on an unexpected reset is 'unclean' */
const char   *bsp_reset_cause_name(reset_cause_t c);

/* stack monitor (main/ISR stack, MSP) */
void     bsp_stack_paint(void);
uint32_t bsp_stack_free_bytes(void);        /* bytes never touched = high-water headroom */
uint32_t bsp_stack_size_bytes(void);

/* shared RAM handshake word survives a soft reset (last 16 bytes of SRAM, excluded from linker) */
typedef struct { uint32_t magic; uint32_t request; uint32_t arg; uint32_t check; } shared_ram_t;
#define SHARED_MAGIC       0x53484D31u
#define SHARED_REQ_NONE    0u
#define SHARED_REQ_UPDATE  1u
shared_ram_t *bsp_shared(void);
bool     bsp_shared_valid(void);
void     bsp_shared_set(uint32_t req, uint32_t arg);
void     bsp_shared_clear(void);

void     bsp_system_reset(void) __attribute__((noreturn));   /* marks the reset as deliberate ('clean') first */
#endif
