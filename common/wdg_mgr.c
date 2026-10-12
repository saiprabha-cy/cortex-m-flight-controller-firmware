#include "wdg_mgr.h"
#include <string.h>

void wdg_init(wdg_t *w, uint32_t grace_ms) { memset(w, 0, sizeof *w); w->grace_ms = grace_ms; }

int wdg_register(wdg_t *w, const char *name, uint32_t deadline_ms, uint32_t now_ms)
{
    for (uint32_t i = 0u; i < WDG_MAX; i++) {
        if (!w->e[i].used) { w->e[i] = (wdg_entry_t){ name, deadline_ms, now_ms, 0u, true, false }; return (int)i; }
    }
    return -1;
}

void wdg_checkin(wdg_t *w, int id, uint32_t now_ms)
{
    if ((id >= 0) && ((uint32_t)id < WDG_MAX)) { w->e[id].last_ms = now_ms; }
}

wdg_result_t wdg_supervise(wdg_t *w, uint32_t now_ms)
{
    wdg_result_t r = { 0u, 0u, false, 0u };
    for (uint32_t i = 0u; i < WDG_MAX; i++) {
        wdg_entry_t *e = &w->e[i];
        if (!e->used) { continue; }
        uint32_t age = now_ms - e->last_ms;                      /* unsigned: wrap-safe */
        if (age > r.worst_age_ms) { r.worst_age_ms = age; }
        if (age > e->deadline_ms) {
            r.late_mask |= (1u << i);
            if (!e->late) { e->late = true; e->miss_count++; r.newly_late_mask |= (1u << i); }
            if (age > (e->deadline_ms + w->grace_ms)) { r.hard_fail = true; }
        } else {
            e->late = false;
        }
    }
    return r;
}
