/* wdg_mgr.h - software task-watchdog supervisor (pure logic, host-testable).
 * Every monitored task calls wdg_checkin() each loop. The supervisor (housekeeping task) calls
 * wdg_supervise() periodically:
 *   - a task late by more than its deadline  -> "soft miss" (reported once, system goes DEGRADED)
 *   - a task late by more than deadline+grace -> hard_fail: the supervisor STOPS kicking the hardware
 *     IWDG so the MCU resets within the IWDG timeout. (Defence in depth: SW detects, HW enforces.)  */
#ifndef WDG_MGR_H
#define WDG_MGR_H
#include <stdint.h>
#include <stdbool.h>
#define WDG_MAX 8u
typedef struct { const char *name; uint32_t deadline_ms, last_ms, miss_count; bool used, late; } wdg_entry_t;
typedef struct { wdg_entry_t e[WDG_MAX]; uint32_t grace_ms; } wdg_t;
typedef struct { uint32_t newly_late_mask, late_mask; bool hard_fail; uint32_t worst_age_ms; } wdg_result_t;

void wdg_init(wdg_t *w, uint32_t grace_ms);
int  wdg_register(wdg_t *w, const char *name, uint32_t deadline_ms, uint32_t now_ms);   /* returns id or -1 */
void wdg_checkin(wdg_t *w, int id, uint32_t now_ms);
wdg_result_t wdg_supervise(wdg_t *w, uint32_t now_ms);
#endif
