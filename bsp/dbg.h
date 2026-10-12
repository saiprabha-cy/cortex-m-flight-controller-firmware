/* dbg.h - tiny printf-style debug console (USART1). No newlib printf, no float: small and deterministic. */
#ifndef DBG_H
#define DBG_H
#include <stdarg.h>
#include "uart.h"
void dbg_init(uart_t *u);                       /* queued (interrupt-driven) output */
void dbg_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void dbg_polled_printf(USART_TypeDef *reg, const char *fmt, ...);   /* fault-safe, no buffers/IRQs */
int  dbg_vsnprintf(char *out, size_t cap, const char *fmt, va_list ap);   /* supports %c %s %d %u %x %X %lu %ld %lx, width, 0-pad */
#endif
