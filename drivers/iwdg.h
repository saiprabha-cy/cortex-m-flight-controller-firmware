#ifndef IWDG_H
#define IWDG_H
#include <stdint.h>
/* Independent watchdog on the ~32 kHz LSI. Once started it cannot be stopped (except by reset). */
void iwdg_start(uint32_t timeout_ms);       /* 8 ms .. ~32 s */
static inline void iwdg_kick(void) { *(volatile uint32_t *)0x40003000u = 0xAAAAu; }
#endif
