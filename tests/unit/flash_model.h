/* flash_model.h - RAM model of the STM32F407 1 MB NOR flash for host tests.
 * Enforces NOR rules (program can only clear bits; erase sets sector to 0xFF) and supports
 * power-failure injection: after N "flash events" the model powers off mid-operation. */
#ifndef FLASH_MODEL_H
#define FLASH_MODEL_H
#include "../../common/flash_if.h"
#include <stdint.h>
extern const flash_ops_t g_flash_model_ops;
void     flash_model_reset(void);                 /* all 0xFF, power on, counters cleared */
void     flash_model_fail_after(long events);     /* -1 = never. each programmed word and each erase = 1 event */
void     flash_model_power_on(void);
long     flash_model_events(void);
uint32_t flash_model_violations(void);            /* attempted 0->1 programs */
uint8_t *flash_model_raw(uint32_t addr);          /* direct pointer (for image placement / corruption) */
uint32_t flash_model_sector_base(uint32_t addr);
uint32_t flash_model_sector_size(uint32_t addr);
#endif
