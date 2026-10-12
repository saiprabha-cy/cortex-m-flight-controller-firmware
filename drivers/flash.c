#include "flash.h"
#include "stm32f4xx.h"
#include "../config/memory_map.h"
#include <string.h>

#define KEY1 0x45670123u
#define KEY2 0xCDEF89ABu
#define SR_ERR_MASK (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR)
#define BSY_TIMEOUT 4000000u        /* generous: a 128 KB sector erase can take > 1 s at 2.7 V */

int flash_hw_sector_of(uint32_t addr, uint32_t *base, uint32_t *size)
{
    if ((addr < FLASH_BASE_ADDR) || (addr >= FLASH_END_ADDR)) { return -1; }
    uint32_t o = addr - FLASH_BASE_ADDR; int n; uint32_t b, s;
    if (o < 0x10000u)      { n = (int)(o >> 14); b = (uint32_t)n << 14; s = 0x4000u; }
    else if (o < 0x20000u) { n = 4; b = 0x10000u; s = 0x10000u; }
    else                   { n = 5 + (int)((o - 0x20000u) >> 17); b = 0x20000u + ((uint32_t)(n - 5) << 17); s = 0x20000u; }
    if (base != NULL) { *base = FLASH_BASE_ADDR + b; }
    if (size != NULL) { *size = s; }
    return n;
}

static int wait_ready(void)
{
    uint32_t n = BSY_TIMEOUT;
    while (((FLASH->SR & FLASH_SR_BSY) != 0u) && (n-- > 0u)) { }
    if (n == 0u) { return FLASH_ERR; }
    return ((FLASH->SR & SR_ERR_MASK) == 0u) ? FLASH_OK : FLASH_ERR;
}
static void unlock(void) { if ((FLASH->CR & FLASH_CR_LOCK) != 0u) { FLASH->KEYR = KEY1; FLASH->KEYR = KEY2; } }
static void lock(void)   { FLASH->CR |= FLASH_CR_LOCK; }
static void clear_flags(void) { FLASH->SR = FLASH_SR_EOP | SR_ERR_MASK; }

static int hw_read(uint32_t addr, void *dst, uint32_t len) { memcpy(dst, (const void *)(uintptr_t)addr, len); return FLASH_OK; }

static int hw_erase(uint32_t addr)
{
    int sec = flash_hw_sector_of(addr, NULL, NULL);
    if (sec < 0) { return FLASH_ERR; }
    unlock(); clear_flags();
    if (wait_ready() != FLASH_OK) { lock(); return FLASH_ERR; }
    FLASH->CR = FLASH_CR_SER | ((uint32_t)sec << FLASH_CR_SNB_Pos) | (2u << FLASH_CR_PSIZE_Pos);   /* PSIZE x32 */
    FLASH->CR |= FLASH_CR_STRT;
    int rc = wait_ready();
    FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB);
    lock();
    return rc;
}

static int hw_program(uint32_t addr, const void *src, uint32_t len)
{
    if (((addr & 3u) != 0u) || ((len & 3u) != 0u) || (flash_hw_sector_of(addr, NULL, NULL) < 0)) { return FLASH_ERR; }
    const uint8_t *p = (const uint8_t *)src; int rc = FLASH_OK;
    unlock(); clear_flags();
    for (uint32_t i = 0u; (i < len) && (rc == FLASH_OK); i += 4u) {
        uint32_t w; memcpy(&w, &p[i], 4u);
        if (wait_ready() != FLASH_OK) { rc = FLASH_ERR; break; }
        FLASH->CR = FLASH_CR_PG | (2u << FLASH_CR_PSIZE_Pos);
        *(volatile uint32_t *)(uintptr_t)(addr + i) = w;
        rc = wait_ready();
        FLASH->CR &= ~FLASH_CR_PG;
        if ((rc == FLASH_OK) && (*(volatile uint32_t *)(uintptr_t)(addr + i) != w)) { rc = FLASH_ERR; }   /* verify */
    }
    lock();
    return rc;
}

const flash_ops_t g_flash_hw_ops = { hw_read, hw_program, hw_erase };
