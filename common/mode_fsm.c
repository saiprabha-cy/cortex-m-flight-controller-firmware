#include "mode_fsm.h"

/* Table-driven so the full transition matrix can be exhaustively unit-tested. */
static const sys_mode_t k_tbl[MODE_COUNT][EV_COUNT] = {
/*                SELF_OK       SELF_FAIL  SENSOR        NODE_LOST     WDG_SOFT      RECOVERED     CRITICAL   WDG_LIMIT  BROWNOUT   CMD_SAFE   CMD_RECOVER */
/* BOOT     */ { MODE_NOMINAL,  MODE_SAFE, MODE_BOOT,    MODE_BOOT,    MODE_BOOT,    MODE_BOOT,    MODE_SAFE, MODE_SAFE, MODE_SAFE, MODE_SAFE, MODE_BOOT },
/* NOMINAL  */ { MODE_NOMINAL,  MODE_SAFE, MODE_DEGRADED,MODE_DEGRADED,MODE_DEGRADED,MODE_NOMINAL, MODE_SAFE, MODE_SAFE, MODE_SAFE, MODE_SAFE, MODE_NOMINAL },
/* DEGRADED */ { MODE_DEGRADED, MODE_SAFE, MODE_DEGRADED,MODE_DEGRADED,MODE_DEGRADED,MODE_NOMINAL, MODE_SAFE, MODE_SAFE, MODE_SAFE, MODE_SAFE, MODE_NOMINAL },
/* SAFE     */ { MODE_SAFE,     MODE_SAFE, MODE_SAFE,    MODE_SAFE,    MODE_SAFE,    MODE_SAFE,    MODE_SAFE, MODE_SAFE, MODE_SAFE, MODE_SAFE, MODE_NOMINAL },
};

sys_mode_t mode_fsm_next(sys_mode_t cur, sys_event_t ev)
{
    if (((unsigned)cur >= (unsigned)MODE_COUNT) || ((unsigned)ev >= (unsigned)EV_COUNT)) { return MODE_SAFE; }
    return k_tbl[cur][ev];
}

const char *mode_name(sys_mode_t m)
{
    static const char *const n[MODE_COUNT] = { "BOOT", "NOMINAL", "DEGRADED", "SAFE" };
    return ((unsigned)m < (unsigned)MODE_COUNT) ? n[m] : "?";
}

const char *event_name(sys_event_t e)
{
    static const char *const n[EV_COUNT] = { "SELF_TEST_OK","SELF_TEST_FAIL","SENSOR_FAULT","NODE_LOST",
        "WDG_SOFT_MISS","RECOVERED","FAULT_CRITICAL","WDG_RESET_LIMIT","BROWNOUT","CMD_SAFE","CMD_RECOVER" };
    return ((unsigned)e < (unsigned)EV_COUNT) ? n[e] : "?";
}
