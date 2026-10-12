#include "flash_model.h"
#include "../../config/memory_map.h"
#include <string.h>
#include <stdlib.h>

static uint8_t  s_mem[FLASH_END_ADDR - FLASH_BASE_ADDR];
static long     s_fail_after = -1;
static long     s_events;
static int      s_powered = 1;
static uint32_t s_violations;

uint32_t flash_model_sector_size(uint32_t addr)
{
    uint32_t o = addr - FLASH_BASE_ADDR;
    if (o < 0x10000u) { return 0x4000u; }
    if (o < 0x20000u) { return 0x10000u; }
    return 0x20000u;
}
uint32_t flash_model_sector_base(uint32_t addr)
{
    uint32_t o = addr - FLASH_BASE_ADDR;
    if (o < 0x10000u) { return FLASH_BASE_ADDR + (o & ~0x3FFFu); }
    if (o < 0x20000u) { return FLASH_BASE_ADDR + 0x10000u; }
    return FLASH_BASE_ADDR + (o & ~0x1FFFFu);
}

void flash_model_reset(void) { memset(s_mem, 0xFF, sizeof s_mem); s_fail_after = -1; s_events = 0; s_powered = 1; s_violations = 0; }
void flash_model_fail_after(long n) { s_fail_after = n; s_events = 0; }
void flash_model_power_on(void) { s_powered = 1; s_fail_after = -1; }
long flash_model_events(void) { return s_events; }
uint32_t flash_model_violations(void) { return s_violations; }
uint8_t *flash_model_raw(uint32_t addr) { return &s_mem[addr - FLASH_BASE_ADDR]; }

static int event(void)       /* returns 1 if power is lost at this event */
{
    if (s_fail_after >= 0 && s_events >= s_fail_after) { s_powered = 0; return 1; }
    s_events++;
    return 0;
}

static int m_read(uint32_t a, void *d, uint32_t n)
{
    if (a < FLASH_BASE_ADDR || (a + n) > FLASH_END_ADDR) { return FLASH_ERR; }
    memcpy(d, &s_mem[a - FLASH_BASE_ADDR], n);
    return FLASH_OK;                                   /* reads always work (like real flash after power-up) */
}

static int m_program(uint32_t a, const void *src, uint32_t n)
{
    if (!s_powered || (a & 3u) || (n & 3u) || a < FLASH_BASE_ADDR || (a + n) > FLASH_END_ADDR) { return FLASH_ERR; }
    const uint8_t *p = (const uint8_t *)src;
    for (uint32_t w = 0u; w < n; w += 4u) {
        if (event()) {                                 /* power lost: this word is half-written garbage */
            for (uint32_t i = 0u; i < 4u; i++) { s_mem[a - FLASH_BASE_ADDR + w + i] &= (uint8_t)(p[w + i] | (uint8_t)(rand())); }
            return FLASH_ERR;
        }
        for (uint32_t i = 0u; i < 4u; i++) {
            uint8_t cur = s_mem[a - FLASH_BASE_ADDR + w + i];
            if ((uint8_t)(cur & p[w + i]) != p[w + i]) { s_violations++; }      /* tried 0 -> 1 */
            s_mem[a - FLASH_BASE_ADDR + w + i] = (uint8_t)(cur & p[w + i]);
        }
    }
    return FLASH_OK;
}

static int m_erase(uint32_t a)
{
    if (!s_powered || a < FLASH_BASE_ADDR || a >= FLASH_END_ADDR) { return FLASH_ERR; }
    uint32_t b = flash_model_sector_base(a), sz = flash_model_sector_size(a);
    if (event()) {                                     /* interrupted erase: half erased, rest random */
        memset(&s_mem[b - FLASH_BASE_ADDR], 0xFF, sz / 2u);
        for (uint32_t i = sz / 2u; i < sz; i++) { s_mem[b - FLASH_BASE_ADDR + i] &= (uint8_t)rand(); }
        return FLASH_ERR;
    }
    memset(&s_mem[b - FLASH_BASE_ADDR], 0xFF, sz);
    return FLASH_OK;
}

const flash_ops_t g_flash_model_ops = { m_read, m_program, m_erase };
