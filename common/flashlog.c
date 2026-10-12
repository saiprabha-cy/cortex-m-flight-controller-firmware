#include "flashlog.h"
#include "crc.h"
#include <string.h>

typedef struct { uint32_t seq; uint8_t payload[FLOG_PAYLOAD_SIZE]; uint32_t crc; } rec_t;

static bool is_erased(const rec_t *r)
{
    const uint8_t *p = (const uint8_t *)r;
    for (uint32_t i = 0u; i < FLOG_REC_SIZE; i++) { if (p[i] != 0xFFu) { return false; } }
    return true;
}

static bool is_valid(const rec_t *r)
{
    return (!is_erased(r)) && (crc32_calc((const uint8_t *)r, FLOG_REC_SIZE - 4u) == r->crc);
}

typedef struct { bool any; uint32_t last_seq; uint32_t next_addr; uint32_t nvalid; bool any_nonff; } scan_t;

static int scan_bank(const flashlog_t *l, uint32_t base, uint32_t size, scan_t *s)
{
    uint32_t nslots = size / FLOG_REC_SIZE;
    uint32_t last_used = 0u;
    bool used = false;
    s->any = false; s->last_seq = 0u; s->nvalid = 0u;
    for (uint32_t i = 0u; i < nslots; i++) {
        rec_t r;
        if (l->fl->read(base + (i * FLOG_REC_SIZE), &r, FLOG_REC_SIZE) != FLASH_OK) { return FLASH_ERR; }
        if (!is_erased(&r)) {
            used = true; last_used = i;                 /* torn/garbage slots count as consumed */
            if (is_valid(&r)) {
                s->nvalid++;
                if ((!s->any) || (r.seq > s->last_seq)) { s->any = true; s->last_seq = r.seq; }
            }
        }
    }
    s->any_nonff = used;
    s->next_addr = used ? (((last_used + 1u) < nslots) ? (base + ((last_used + 1u) * FLOG_REC_SIZE)) : 0u) : base;
    return FLASH_OK;
}

int flashlog_init(flashlog_t *l, const flash_ops_t *fl, uint32_t base0, uint32_t size0,
                  uint32_t base1, uint32_t size1)
{
    scan_t s[2];
    memset(l, 0, sizeof *l);
    l->fl = fl; l->base[0] = base0; l->size[0] = size0; l->base[1] = base1; l->size[1] = size1;
    l->nbanks = (base1 != 0u) ? 2u : 1u;
    for (uint8_t b = 0u; b < l->nbanks; b++) {
        if (scan_bank(l, l->base[b], l->size[b], &s[b]) != FLASH_OK) { return FLASH_ERR; }
    }
    l->active = 0u;
    if (l->nbanks == 2u) {
        if (s[0].any && s[1].any) {                      /* interrupted switch-over: keep newest, clean the other */
            l->active = (s[1].last_seq > s[0].last_seq) ? 1u : 0u;
            uint8_t old = (uint8_t)(1u - l->active);
            if (fl->erase_sector(l->base[old]) != FLASH_OK) { return FLASH_ERR; }
            if (scan_bank(l, l->base[old], l->size[old], &s[old]) != FLASH_OK) { return FLASH_ERR; }
        } else if (s[1].any || (s[1].any_nonff && !s[0].any)) {
            l->active = 1u;
        }
    }
    l->have_record = s[l->active].any;
    l->last_seq = s[l->active].last_seq;
    l->next_addr = s[l->active].next_addr;
    return FLASH_OK;
}

static int write_rec(const flashlog_t *l, uint32_t addr, uint32_t seq, const uint8_t *payload)
{
    rec_t r;
    r.seq = seq;
    memcpy(r.payload, payload, FLOG_PAYLOAD_SIZE);
    r.crc = crc32_calc((const uint8_t *)&r, FLOG_REC_SIZE - 4u);
    return l->fl->program(addr, &r, FLOG_REC_SIZE);
}

int flashlog_append(flashlog_t *l, const uint8_t payload[FLOG_PAYLOAD_SIZE])
{
    uint32_t seq = l->have_record ? (l->last_seq + 1u) : 1u;

    if (l->next_addr == 0u) {                            /* active bank full */
        if (l->nbanks == 2u) {
            uint8_t other = (uint8_t)(1u - l->active);
            rec_t probe;
            if (l->fl->read(l->base[other], &probe, FLOG_REC_SIZE) != FLASH_OK) { return FLASH_ERR; }
            if (!is_erased(&probe)) {
                if (l->fl->erase_sector(l->base[other]) != FLASH_OK) { return FLASH_ERR; }
            }
            if (write_rec(l, l->base[other], seq, payload) != FLASH_OK) { return FLASH_ERR; }
            if (l->fl->erase_sector(l->base[l->active]) != FLASH_OK) { return FLASH_ERR; }
            l->active = other;
            l->next_addr = l->base[other] + FLOG_REC_SIZE;
            l->last_seq = seq; l->have_record = true;
            return FLASH_OK;
        }
        if (l->fl->erase_sector(l->base[0]) != FLASH_OK) { return FLASH_ERR; }   /* single bank wrap */
        l->next_addr = l->base[0];
    }

    uint32_t addr = l->next_addr;
    if (write_rec(l, addr, seq, payload) != FLASH_OK) {
        /* A failed/torn program leaves the slot consumed; move on so we never reprogram it. */
        l->next_addr = ((addr + FLOG_REC_SIZE) < (l->base[l->active] + l->size[l->active])) ? (addr + FLOG_REC_SIZE) : 0u;
        return FLASH_ERR;
    }
    l->last_seq = seq; l->have_record = true;
    addr += FLOG_REC_SIZE;
    l->next_addr = (addr < (l->base[l->active] + l->size[l->active])) ? addr : 0u;
    return FLASH_OK;
}

bool flashlog_latest(const flashlog_t *l, uint8_t payload[FLOG_PAYLOAD_SIZE], uint32_t *seq)
{
    uint32_t n = flashlog_count(l);
    return (n > 0u) ? flashlog_read(l, n - 1u, payload, seq) : false;
}

bool flashlog_read(const flashlog_t *l, uint32_t idx, uint8_t payload[FLOG_PAYLOAD_SIZE], uint32_t *seq)
{
    uint32_t nslots = l->size[l->active] / FLOG_REC_SIZE;
    uint32_t seen = 0u;
    for (uint32_t i = 0u; i < nslots; i++) {
        rec_t r;
        if (l->fl->read(l->base[l->active] + (i * FLOG_REC_SIZE), &r, FLOG_REC_SIZE) != FLASH_OK) { return false; }
        if (is_valid(&r)) {
            if (seen == idx) { memcpy(payload, r.payload, FLOG_PAYLOAD_SIZE); if (seq != NULL) { *seq = r.seq; } return true; }
            seen++;
        }
    }
    return false;
}

uint32_t flashlog_count(const flashlog_t *l)
{
    scan_t s;
    return (scan_bank(l, l->base[l->active], l->size[l->active], &s) == FLASH_OK) ? s.nvalid : 0u;
}
