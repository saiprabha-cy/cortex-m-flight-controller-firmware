#include "boot_meta.h"
#include <string.h>

void boot_meta_default(boot_meta_t *m)
{
    memset(m, 0, sizeof *m);
    m->active_slot = SLOT_A;
    m->slot_state[SLOT_A] = SLOT_STATE_CONFIRMED;   /* factory: assume A is the golden image if it verifies */
    m->slot_state[SLOT_B] = SLOT_STATE_EMPTY;
}

int boot_meta_load(const flashlog_t *log, boot_meta_t *m)
{
    uint8_t p[FLOG_PAYLOAD_SIZE];
    uint32_t seq;
    if (flashlog_latest(log, p, &seq)) { memcpy(m, p, sizeof *m); } else { boot_meta_default(m); }
    if (m->active_slot > SLOT_B) { boot_meta_default(m); }
    return FLASH_OK;
}

int boot_meta_save(flashlog_t *log, const boot_meta_t *m)
{
    uint8_t p[FLOG_PAYLOAD_SIZE];
    memcpy(p, m, sizeof *m);
    return flashlog_append(log, p);
}

int boot_decide(boot_meta_t *m, verify_fn_t verify, void *ctx)
{
    if ((m->flags & META_FLAG_UPDATE_REQUEST) != 0u) { return BOOT_DECISION_UPDATE; }

    const uint32_t order[2] = { m->active_slot, 1u - m->active_slot };
    for (uint32_t i = 0u; i < 2u; i++) {
        uint32_t s = order[i];
        if ((m->slot_state[s] == SLOT_STATE_BAD) || (m->slot_state[s] == SLOT_STATE_EMPTY && s != m->active_slot)) { continue; }
        if ((m->slot_state[s] == SLOT_STATE_TRIAL) && (s == m->active_slot) && (m->attempts >= BOOT_MAX_ATTEMPTS)) {
            m->slot_state[s] = SLOT_STATE_BAD;           /* never confirmed after N boots -> roll back */
            continue;
        }
        img_status_t st = verify(s, m->min_sec_counter, ctx);
        if (st != IMG_OK) { m->slot_state[s] = SLOT_STATE_BAD; m->last_boot_result = (uint8_t)st; continue; }

        if (s != m->active_slot) { m->active_slot = (uint8_t)s; m->attempts = 0u; }   /* fell back */
        if (m->slot_state[s] == SLOT_STATE_TRIAL) { m->attempts++; }
        if (m->slot_state[s] == SLOT_STATE_EMPTY) { m->slot_state[s] = SLOT_STATE_CONFIRMED; }
        m->boot_count++;
        return (int)s;
    }
    return BOOT_DECISION_NONE;
}

void boot_meta_mark_installed(boot_meta_t *m, uint32_t slot, uint32_t version)
{
    m->slot_state[slot] = SLOT_STATE_TRIAL;
    m->active_slot = (uint8_t)slot;
    m->attempts = 0u;
    m->flags &= (uint8_t)~META_FLAG_UPDATE_REQUEST;
    m->version[slot] = version;
}

void boot_meta_mark_confirmed(boot_meta_t *m, uint32_t slot, uint32_t sec_counter)
{
    m->slot_state[slot] = SLOT_STATE_CONFIRMED;
    m->attempts = 0u;
    if (sec_counter > m->min_sec_counter) { m->min_sec_counter = sec_counter; }
}
