#include "dbg.h"
#include <string.h>

static uart_t *s_dbg;
void dbg_init(uart_t *u) { s_dbg = u; }

static size_t put_num(char *o, size_t cap, size_t pos, uint32_t v, uint32_t base, bool neg, int width, bool zpad, bool upper)
{
    char tmp[12]; int n = 0;
    if (v == 0u) { tmp[n++] = '0'; }
    while (v != 0u) { uint32_t d = v % base; tmp[n++] = (char)((d < 10u) ? ('0' + d) : ((upper ? 'A' : 'a') + (d - 10u))); v /= base; }
    int total = n + (neg ? 1 : 0);
    if (!zpad) { for (int i = total; i < width; i++) { if (pos + 1u < cap) { o[pos] = ' '; } pos++; } }
    if (neg) { if (pos + 1u < cap) { o[pos] = '-'; } pos++; }
    if (zpad) { for (int i = total; i < width; i++) { if (pos + 1u < cap) { o[pos] = '0'; } pos++; } }
    while (n > 0) { n--; if (pos + 1u < cap) { o[pos] = tmp[n]; } pos++; }
    return pos;
}

int dbg_vsnprintf(char *o, size_t cap, const char *f, va_list ap)
{
    size_t pos = 0u;
    for (; *f != '\0'; f++) {
        if (*f != '%') { if (pos + 1u < cap) { o[pos] = *f; } pos++; continue; }
        f++;
        bool zpad = false; int width = 0; bool lng = false;
        if (*f == '0') { zpad = true; f++; }
        while ((*f >= '0') && (*f <= '9')) { width = (width * 10) + (*f - '0'); f++; }
        if (*f == 'l') { lng = true; f++; }
        switch (*f) {
        case 'c': if (pos + 1u < cap) { o[pos] = (char)va_arg(ap, int); } pos++; break;
        case 's': { const char *s = va_arg(ap, const char *); if (s == NULL) { s = "(null)"; } while (*s != '\0') { if (pos + 1u < cap) { o[pos] = *s; } pos++; s++; } break; }
        case 'd': { int32_t v = lng ? (int32_t)va_arg(ap, long) : va_arg(ap, int); bool n = v < 0; pos = put_num(o, cap, pos, n ? (uint32_t)(-(int64_t)v) : (uint32_t)v, 10u, n, width, zpad, false); break; }
        case 'u': { uint32_t v = lng ? (uint32_t)va_arg(ap, unsigned long) : va_arg(ap, unsigned); pos = put_num(o, cap, pos, v, 10u, false, width, zpad, false); break; }
        case 'x': case 'X': { uint32_t v = lng ? (uint32_t)va_arg(ap, unsigned long) : va_arg(ap, unsigned); pos = put_num(o, cap, pos, v, 16u, false, width, zpad, *f == 'X'); break; }
        case '%': if (pos + 1u < cap) { o[pos] = '%'; } pos++; break;
        default: break;
        }
    }
    if (cap > 0u) { o[(pos < cap) ? pos : (cap - 1u)] = '\0'; }
    return (int)pos;
}

void dbg_printf(const char *fmt, ...)
{
    char buf[160]; va_list ap; va_start(ap, fmt);
    int n = dbg_vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    if ((s_dbg != NULL) && (n > 0)) { uart_write(s_dbg, (const uint8_t *)buf, (size_t)((n < (int)sizeof buf) ? n : (int)sizeof buf - 1)); }
}

void dbg_polled_printf(USART_TypeDef *reg, const char *fmt, ...)
{
    char buf[160]; va_list ap; va_start(ap, fmt);
    (void)dbg_vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    uart_puts_polled(reg, buf);
}
