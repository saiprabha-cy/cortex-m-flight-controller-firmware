/* flashlog.h - power-fail-safe, append-only record log on NOR flash.
 *
 * Record (32 bytes): [seq:u32][payload:24][crc32 over seq+payload:u32]; erased = all 0xFF.
 * One or two banks. With two banks the log ping-pongs: when the active bank is full the newest
 * record is written to the other (already erased) bank BEFORE the old bank is erased, so a power
 * cut at any point leaves at least one valid copy of the newest record.
 * With one bank (fault log) a wrap erases the bank (documented trade-off, see FMEA FM-07). */
#ifndef FLASHLOG_H
#define FLASHLOG_H
#include <stdint.h>
#include <stdbool.h>
#include "flash_if.h"

#define FLOG_REC_SIZE      32u
#define FLOG_PAYLOAD_SIZE  24u

typedef struct {
    const flash_ops_t *fl;
    uint32_t base[2];
    uint32_t size[2];
    uint8_t  nbanks;
    uint8_t  active;
    uint32_t next_addr;     /* where the next record goes (0 = bank full) */
    uint32_t last_seq;
    bool     have_record;
} flashlog_t;

int  flashlog_init(flashlog_t *l, const flash_ops_t *fl, uint32_t base0, uint32_t size0,
                   uint32_t base1, uint32_t size1);       /* base1 = 0 -> single bank */
int  flashlog_append(flashlog_t *l, const uint8_t payload[FLOG_PAYLOAD_SIZE]);
bool flashlog_latest(const flashlog_t *l, uint8_t payload[FLOG_PAYLOAD_SIZE], uint32_t *seq);
/* Reads the idx-th valid record (0 = oldest) of the active bank. Returns false when idx is out of range. */
bool flashlog_read(const flashlog_t *l, uint32_t idx, uint8_t payload[FLOG_PAYLOAD_SIZE], uint32_t *seq);
uint32_t flashlog_count(const flashlog_t *l);
#endif
