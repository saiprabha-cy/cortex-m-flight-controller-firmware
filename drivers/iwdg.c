#include "iwdg.h"
#include "stm32f4xx.h"

void iwdg_start(uint32_t timeout_ms)
{
    /* prescaler /32 -> 1 kHz tick (LSI nominal 32 kHz, +-tolerance is large: see FMEA FM-05) */
    if (timeout_ms < 8u) { timeout_ms = 8u; }
    if (timeout_ms > 4095u) { timeout_ms = 4095u; }
    IWDG->KR = 0x5555u;
    IWDG->PR = 3u;                       /* /32 */
    IWDG->RLR = timeout_ms;
    IWDG->KR = 0xAAAAu;
    IWDG->KR = 0xCCCCu;                  /* start */
}
