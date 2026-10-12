#include "bsp.h"
#include "stm32f4xx.h"
#include "../config/memory_map.h"

uint32_t g_sysclk_hz = 16000000u, g_pclk1_hz = 16000000u, g_pclk2_hz = 16000000u;
extern uint32_t _ebss, _estack;

#define CLK_TIMEOUT_LOOPS 200000u

static bool wait_flag(volatile uint32_t *reg, uint32_t mask, bool want_set)
{
    for (uint32_t i = 0u; i < CLK_TIMEOUT_LOOPS; i++) {
        if (((*reg & mask) != 0u) == want_set) { return true; }
    }
    return false;                                  /* bounded: never hang in clock bring-up */
}

/* 8 MHz HSE / M(8) * N(336) / P(2) = 168 MHz; Q(7) -> 48 MHz; AHB /1, APB1 /4 (42 MHz), APB2 /2 (84 MHz) */
clk_status_t bsp_clock_init(void)
{
    RCC->CR |= RCC_CR_HSEON;
    if (!wait_flag(&RCC->CR, RCC_CR_HSERDY, true)) { goto fallback; }

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;                                           /* voltage scale 1 for 168 MHz */
    FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_5WS;
    RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
    RCC->PLLCFGR = (8u << RCC_PLLCFGR_PLLM_Pos) | (336u << RCC_PLLCFGR_PLLN_Pos) | (0u << RCC_PLLCFGR_PLLP_Pos) |
                   RCC_PLLCFGR_PLLSRC_HSE | (7u << RCC_PLLCFGR_PLLQ_Pos);
    RCC->CR |= RCC_CR_PLLON;
    if (!wait_flag(&RCC->CR, RCC_CR_PLLRDY, true)) { goto fallback; }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    if (!wait_flag(&RCC->CFGR, RCC_CFGR_SWS_PLL, true)) { goto fallback; }

    g_sysclk_hz = 168000000u; g_pclk1_hz = 42000000u; g_pclk2_hz = 84000000u;
    return CLK_PLL_168MHZ;

fallback:                                           /* fail-safe: run from the internal 16 MHz oscillator */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON);
    FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_0WS;
    RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV1 | RCC_CFGR_PPRE2_DIV1;
    g_sysclk_hz = g_pclk1_hz = g_pclk2_hz = 16000000u;
    return CLK_HSI_FALLBACK;
}

/* ---- TIM2: 32-bit up-counter at 1 MHz (APB1 timer clock = 2 x PCLK1 when PCLK1 prescaler != 1) ---- */
void bsp_timebase_init(void)
{
    uint32_t tim_clk = (g_pclk1_hz == g_sysclk_hz) ? g_pclk1_hz : (2u * g_pclk1_hz);
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    TIM2->PSC = (tim_clk / 1000000u) - 1u;
    TIM2->ARR = 0xFFFFFFFFu;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CR1 = TIM_CR1_CEN;
}
uint32_t time_us(void) { return TIM2->CNT; }
uint32_t time_ms(void) { return TIM2->CNT / 1000u; }           /* wraps every ~71 min; use time_us() deltas for intervals */
void delay_us(uint32_t us) { uint32_t t0 = TIM2->CNT; while ((uint32_t)(TIM2->CNT - t0) < us) { } }
void delay_ms(uint32_t ms) { while (ms-- > 0u) { delay_us(1000u); } }

bool g_dwt_hw;
void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    /* Probe: does the counter really run? (Renode 1.17 has no DWT model; QEMU/real silicon do.) */
    uint32_t a = DWT->CYCCNT; for (volatile uint32_t i = 0u; i < 50u; i++) { } uint32_t b = DWT->CYCCNT;
    g_dwt_hw = (b != a);
}
uint32_t dwt_cycles(void) { return g_dwt_hw ? DWT->CYCCNT : (TIM2->CNT * (g_sysclk_hz / 1000000u)); }
uint32_t dwt_cycles_to_ns(uint32_t c) { return (uint32_t)(((uint64_t)c * 1000000000ull) / g_sysclk_hz); }

typedef struct { uint32_t magic, cause_latched, run_state, check; } boot_info_t;
#define BOOT_INFO_MAGIC 0x42494E46u
enum { RUN_UNKNOWN = 0, RUN_RUNNING = 1, RUN_CLEAN_RESET = 2 };
static volatile boot_info_t *bi(void) { return (volatile boot_info_t *)BOOT_INFO_ADDR; }
static bool bi_valid(void) { volatile boot_info_t *b = bi(); return (b->magic == BOOT_INFO_MAGIC) && (b->check == (b->magic ^ b->cause_latched ^ b->run_state ^ 0x5Au)); }
static void bi_store(uint32_t latched, uint32_t run)
{ volatile boot_info_t *b = bi(); b->magic = BOOT_INFO_MAGIC; b->cause_latched = latched; b->run_state = run; b->check = BOOT_INFO_MAGIC ^ latched ^ run ^ 0x5Au; }

#ifndef FC_EMU_RESET_INFERENCE
#define FC_EMU_RESET_INFERENCE 1
#endif

reset_cause_t bsp_reset_cause(void)
{
    if (bi_valid() && (bi()->cause_latched != 0u)) {            /* already read (and RCC flags cleared) by the bootloader */
        reset_cause_t c = (reset_cause_t)(bi()->cause_latched - 1u);
        bi_store(0u, bi()->run_state);
        return c;
    }
    uint32_t csr = RCC->CSR; reset_cause_t c = RST_UNKNOWN;
    if ((csr & RCC_CSR_WDGRSTF) != 0u)        { c = RST_IWDG; }
    else if ((csr & RCC_CSR_WWDGRSTF) != 0u)  { c = RST_WWDG; }
    else if ((csr & RCC_CSR_LPWRRSTF) != 0u)  { c = RST_LOW_POWER; }
    else if ((csr & RCC_CSR_SFTRSTF) != 0u)   { c = RST_SOFTWARE; }
    else if ((csr & RCC_CSR_BORRSTF) != 0u)   { c = RST_BROWN_OUT; }          /* BORRSTF is also set by POR on F4: checked by PORRSTF below */
    else if ((csr & RCC_CSR_PORRSTF) != 0u)   { c = RST_POWER_ON; }
    else if ((csr & RCC_CSR_PINRSTF) != 0u)   { c = RST_PIN; }
    if ((c == RST_BROWN_OUT) && ((csr & RCC_CSR_PORRSTF) != 0u)) { c = RST_POWER_ON; }
#if FC_EMU_RESET_INFERENCE
    if (((c == RST_POWER_ON) || (c == RST_PIN) || (c == RST_UNKNOWN)) && bi_valid()) {
        if (bi()->run_state == RUN_RUNNING)          { c = RST_IWDG; }       /* app was running and did not request this reset */
        else if (bi()->run_state == RUN_CLEAN_RESET) { c = RST_SOFTWARE; }
    }
#endif
    RCC->CSR |= RCC_CSR_RMVF;
    return c;
}
void bsp_reset_cause_latch(reset_cause_t c) { bi_store((uint32_t)c + 1u, bi_valid() ? bi()->run_state : (uint32_t)RUN_UNKNOWN); }
void bsp_mark_running(void) { bi_store(bi_valid() ? bi()->cause_latched : 0u, RUN_RUNNING); }
const char *bsp_reset_cause_name(reset_cause_t c)
{
    static const char *const n[] = { "UNKNOWN", "POWER_ON", "BROWN_OUT", "PIN", "SOFTWARE", "IWDG", "WWDG", "LOW_POWER" };
    return ((unsigned)c < 8u) ? n[c] : "?";
}

#define CANARY 0xDEADBEEFu
void bsp_stack_paint(void)
{
    uint32_t sp = (uint32_t)__get_MSP();
    for (uint32_t *p = &_ebss; (uint32_t)p < (sp - 64u); p++) { *p = CANARY; }
}
uint32_t bsp_stack_free_bytes(void)
{
    uint32_t *p = &_ebss;
    while ((p < &_estack) && (*p == CANARY)) { p++; }
    return (uint32_t)((uintptr_t)p - (uintptr_t)&_ebss);
}
uint32_t bsp_stack_size_bytes(void) { return (uint32_t)((uintptr_t)&_estack - (uintptr_t)&_ebss); }

shared_ram_t *bsp_shared(void) { return (shared_ram_t *)SHARED_RAM_ADDR; }
bool bsp_shared_valid(void)
{
    const volatile shared_ram_t *s = bsp_shared();
    return (s->magic == SHARED_MAGIC) && (s->check == (s->magic ^ s->request ^ s->arg ^ 0xA5A5A5A5u));
}
void bsp_shared_set(uint32_t req, uint32_t arg)
{
    volatile shared_ram_t *s = bsp_shared();
    s->magic = SHARED_MAGIC; s->request = req; s->arg = arg; s->check = SHARED_MAGIC ^ req ^ arg ^ 0xA5A5A5A5u;
}
void bsp_shared_clear(void) { volatile shared_ram_t *s = bsp_shared(); s->magic = 0u; s->request = 0u; s->arg = 0u; s->check = 0u; }

void bsp_system_reset(void) { bi_store(bi_valid() ? bi()->cause_latched : 0u, RUN_CLEAN_RESET); __DSB(); NVIC_SystemReset(); for (;;) { } }
