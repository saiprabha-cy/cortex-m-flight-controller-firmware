/* boot_meta.h - persistent boot state + the pure boot-decision logic (host-testable). */
#ifndef BOOT_META_H
#define BOOT_META_H
#include <stdint.h>
#include <stdbool.h>
#include "flashlog.h"
#include "image_verify.h"

typedef enum { SLOT_STATE_EMPTY = 0, SLOT_STATE_TRIAL = 1, SLOT_STATE_CONFIRMED = 2, SLOT_STATE_BAD = 3 } slot_state_t;

#define META_FLAG_UPDATE_REQUEST  0x01u
#define BOOT_MAX_ATTEMPTS         3u

/* 24 bytes = exactly one flashlog payload */
typedef struct __attribute__((packed)) {
    uint8_t  active_slot;
    uint8_t  slot_state[2];
    uint8_t  attempts;          /* boots of the active slot while it is still in TRIAL */
    uint8_t  flags;
    uint8_t  last_boot_result;  /* img_status_t of the most recent failed verification */
    uint16_t reserved;
    uint32_t min_sec_counter;   /* anti-rollback floor */
    uint32_t boot_count;
    uint32_t version[2];        /* version of the image last installed in each slot */
} boot_meta_t;
_Static_assert(sizeof(boot_meta_t) == FLOG_PAYLOAD_SIZE, "boot_meta_t must be 24 bytes");

void boot_meta_default(boot_meta_t *m);
int  boot_meta_load(const flashlog_t *log, boot_meta_t *m);   /* returns FLASH_OK; m = defaults if log empty */
int  boot_meta_save(flashlog_t *log, const boot_meta_t *m);

#define BOOT_DECISION_NONE    (-1)    /* no bootable image -> recovery */
#define BOOT_DECISION_UPDATE  (-2)    /* update requested -> stay in bootloader */

typedef img_status_t (*verify_fn_t)(uint32_t slot, uint32_t min_sec_counter, void *ctx);

/* Decide which slot to boot; updates *m (attempt counters, BAD marks, active slot). */
int boot_decide(boot_meta_t *m, verify_fn_t verify, void *ctx);

/* Called by the bootloader after a successful install into `slot`. */
void boot_meta_mark_installed(boot_meta_t *m, uint32_t slot, uint32_t version);
/* Called by the application once it has proven itself healthy. */
void boot_meta_mark_confirmed(boot_meta_t *m, uint32_t slot, uint32_t sec_counter);
#endif
